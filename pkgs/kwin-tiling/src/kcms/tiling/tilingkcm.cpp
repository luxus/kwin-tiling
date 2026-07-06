/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 KWin Tiling Fork
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "tilingkcm.h"

#include "tilingsettings.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KSharedConfig>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QGuiApplication>
#include <QScreen>
#include <QVariantMap>
#include <qqml.h>

K_PLUGIN_FACTORY_WITH_JSON(TilingKCMFactory, "kcm_kwin_tiling.json", registerPlugin<KWin::TilingKCM>();)

namespace KWin
{

namespace
{
constexpr int kDefaultGapLeft = 0;
constexpr int kDefaultGapRight = 0;
constexpr int kDefaultGapTop = 0;
constexpr int kDefaultGapBottom = 0;
constexpr int kDefaultGapBetween = 0;

// A human-readable label for a monitor (model, else "<vendor> <name>", else name).
QString screenDescription(const QScreen *screen)
{
    if (!screen) {
        return {};
    }
    QString description = screen->model();
    if (description.isEmpty()) {
        description = QStringLiteral("%1 %2").arg(screen->manufacturer(), screen->name());
    }
    if (description.trimmed().isEmpty()) {
        description = screen->name();
    }
    return description;
}
} // namespace

OutputGapOverride::OutputGapOverride(QString name, QString description, int gapLeft, int gapRight,
                                     int gapTop, int gapBottom, int gapBetween, QString defaultLayout,
                                     qreal masterRatio, int masterCount, qreal defaultColumnWidth,
                                     QObject *parent)
    : QObject(parent)
    , m_name(std::move(name))
    , m_description(std::move(description))
    , m_gapLeft(gapLeft)
    , m_gapRight(gapRight)
    , m_gapTop(gapTop)
    , m_gapBottom(gapBottom)
    , m_gapBetween(gapBetween)
    , m_defaultLayout(std::move(defaultLayout))
    , m_masterRatio(masterRatio)
    , m_masterCount(masterCount)
    , m_defaultColumnWidth(defaultColumnWidth)
{
}

void OutputGapOverride::setGapLeft(int value)
{
    if (m_gapLeft == value) {
        return;
    }
    m_gapLeft = value;
    Q_EMIT gapLeftChanged();
    Q_EMIT modified();
}

void OutputGapOverride::setGapRight(int value)
{
    if (m_gapRight == value) {
        return;
    }
    m_gapRight = value;
    Q_EMIT gapRightChanged();
    Q_EMIT modified();
}

void OutputGapOverride::setGapTop(int value)
{
    if (m_gapTop == value) {
        return;
    }
    m_gapTop = value;
    Q_EMIT gapTopChanged();
    Q_EMIT modified();
}

void OutputGapOverride::setGapBottom(int value)
{
    if (m_gapBottom == value) {
        return;
    }
    m_gapBottom = value;
    Q_EMIT gapBottomChanged();
    Q_EMIT modified();
}

void OutputGapOverride::setGapBetween(int value)
{
    if (m_gapBetween == value) {
        return;
    }
    m_gapBetween = value;
    Q_EMIT gapBetweenChanged();
    Q_EMIT modified();
}

void OutputGapOverride::setDefaultLayout(const QString &value)
{
    // Always emit modified so the KCM apply button reliably lights up
    // when the user touches the control, even if the value happens to
    // resolve to the same string (e.g. they re-selected it).
    m_defaultLayout = value;
    Q_EMIT defaultLayoutChanged();
    Q_EMIT modified();
}

void OutputGapOverride::setMasterRatio(qreal value)
{
    if (qFuzzyCompare(m_masterRatio, value)) {
        return;
    }
    m_masterRatio = value;
    Q_EMIT masterRatioChanged();
    Q_EMIT modified();
}

void OutputGapOverride::setMasterCount(int value)
{
    if (m_masterCount == value) {
        return;
    }
    m_masterCount = value;
    Q_EMIT masterCountChanged();
    Q_EMIT modified();
}

void OutputGapOverride::setDefaultColumnWidth(qreal value)
{
    if (qFuzzyCompare(m_defaultColumnWidth, value)) {
        return;
    }
    m_defaultColumnWidth = value;
    Q_EMIT defaultColumnWidthChanged();
    Q_EMIT modified();
}

OutputGapOverridesModel::OutputGapOverridesModel(QObject *parent)
    : QAbstractListModel(parent)
{
    if (qApp) {
        // Connecting/disconnecting a monitor changes which ones are available
        // to add a custom override for. (Signal-to-signal: the QScreen* arg is
        // dropped.)
        connect(qApp, &QGuiApplication::screenAdded, this, &OutputGapOverridesModel::availableMonitorsChanged);
        connect(qApp, &QGuiApplication::screenRemoved, this, &OutputGapOverridesModel::availableMonitorsChanged);
    }
}

OutputGapOverridesModel::~OutputGapOverridesModel()
{
    qDeleteAll(m_entries);
}

QHash<int, QByteArray> OutputGapOverridesModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {DescriptionRole, "description"},
        {GapLeftRole, "gapLeft"},
        {GapRightRole, "gapRight"},
        {GapTopRole, "gapTop"},
        {GapBottomRole, "gapBottom"},
        {GapBetweenRole, "gapBetween"},
        {DefaultLayoutRole, "defaultLayout"},
        {EntryRole, "entry"},
    };
}

QVariant OutputGapOverridesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }
    OutputGapOverride *entry = m_entries.at(index.row());
    switch (role) {
    case NameRole:
        return entry->name();
    case DescriptionRole:
        return entry->description();
    case GapLeftRole:
        return entry->gapLeft();
    case GapRightRole:
        return entry->gapRight();
    case GapTopRole:
        return entry->gapTop();
    case GapBottomRole:
        return entry->gapBottom();
    case GapBetweenRole:
        return entry->gapBetween();
    case DefaultLayoutRole:
        return entry->defaultLayout();
    case EntryRole:
        return QVariant::fromValue(entry);
    }
    return {};
}

int OutputGapOverridesModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_entries.size();
}

QVariantList OutputGapOverridesModel::availableMonitors() const
{
    QVariantList result;
    if (!qApp) {
        return result;
    }
    for (const QScreen *screen : qApp->screens()) {
        if (!screen || entryForName(screen->name())) {
            continue; // skip already-customized monitors
        }
        result.append(QVariantMap{
            {QStringLiteral("name"), screen->name()},
            {QStringLiteral("description"), screenDescription(screen)},
        });
    }
    return result;
}

OutputGapOverride *OutputGapOverridesModel::entryForName(const QString &name) const
{
    for (OutputGapOverride *entry : m_entries) {
        if (entry->name() == name) {
            return entry;
        }
    }
    return nullptr;
}

void OutputGapOverridesModel::addEntry(const QString &name, const QString &description, int left, int right,
                                       int top, int bottom, int between, const QString &defaultLayout,
                                       qreal masterRatio, int masterCount, qreal defaultColumnWidth)
{
    auto *entry = new OutputGapOverride(name, description, left, right, top, bottom, between, defaultLayout,
                                        masterRatio, masterCount, defaultColumnWidth, this);
    connect(entry, &OutputGapOverride::modified, this, [this, entry]() {
        if (!m_modified) {
            setModified(true);
        }
    });
    m_entries.append(entry);
}

void OutputGapOverridesModel::setModified(bool modified)
{
    if (m_modified == modified) {
        return;
    }
    m_modified = modified;
    Q_EMIT modifiedChanged();
}

void OutputGapOverridesModel::load(KConfigGroup &tilingGroup, const TilingSettings *settings)
{
    // Per-key fallbacks let an older, partial "Output" sub-group (which only
    // stored keys that differed from the defaults) still load cleanly.
    const int defaultLeft = settings ? settings->gapLeft() : kDefaultGapLeft;
    const int defaultRight = settings ? settings->gapRight() : kDefaultGapRight;
    const int defaultTop = settings ? settings->gapTop() : kDefaultGapTop;
    const int defaultBottom = settings ? settings->gapBottom() : kDefaultGapBottom;
    const int defaultBetween = settings ? settings->gapBetween() : kDefaultGapBetween;
    const QString defaultLayout = settings ? settings->defaultLayout() : QStringLiteral("MasterStack");
    const qreal defaultMasterRatio = settings ? settings->masterRatio() : 0.5;
    const int defaultMasterCount = settings ? settings->masterCount() : 1;
    const qreal defaultColumnWidth = settings ? settings->defaultColumnWidth() : 0.5;

    beginResetModel();
    qDeleteAll(m_entries);
    m_entries.clear();

    // One entry per existing "Output <name>" sub-group = a customized monitor.
    // Monitors without a sub-group simply follow the defaults (no entry).
    const QStringList subGroups = tilingGroup.groupList();
    for (const QString &sub : subGroups) {
        if (!sub.startsWith(QLatin1String("Output "))) {
            continue;
        }
        const QString name = sub.mid(QStringLiteral("Output ").size());
        if (name.isEmpty()) {
            continue;
        }
        const KConfigGroup outputGroup = tilingGroup.group(sub);
        const int left = outputGroup.readEntry("GapLeft", defaultLeft);
        const int right = outputGroup.readEntry("GapRight", defaultRight);
        const int top = outputGroup.readEntry("GapTop", defaultTop);
        const int bottom = outputGroup.readEntry("GapBottom", defaultBottom);
        const int between = outputGroup.readEntry("GapBetween", defaultBetween);
        const QString entryLayout = outputGroup.readEntry("DefaultLayout", defaultLayout);
        const qreal masterRatio = outputGroup.readEntry("MasterRatio", defaultMasterRatio);
        const int masterCount = outputGroup.readEntry("MasterCount", defaultMasterCount);
        const qreal columnWidth = outputGroup.readEntry("DefaultColumnWidth", defaultColumnWidth);

        // Use the connected screen's label if that monitor is currently attached.
        QString description = name;
        if (qApp) {
            for (const QScreen *screen : qApp->screens()) {
                if (screen && screen->name() == name) {
                    description = screenDescription(screen);
                    break;
                }
            }
        }

        addEntry(name, description, left, right, top, bottom, between, entryLayout,
                 masterRatio, masterCount, columnWidth);
    }

    endResetModel();
    Q_EMIT countChanged();
    Q_EMIT availableMonitorsChanged();
    setModified(false);
}

void OutputGapOverridesModel::save(KConfigGroup &tilingGroup, const TilingSettings *settings)
{
    Q_UNUSED(settings)

    // Replace all per-output sub-groups: one per customized monitor, with every
    // value written so the customization persists (its presence is what marks
    // the monitor as customized on the next load).
    const QStringList existing = tilingGroup.groupList();
    for (const QString &sub : existing) {
        if (sub.startsWith(QLatin1String("Output "))) {
            tilingGroup.deleteGroup(sub);
        }
    }

    for (OutputGapOverride *entry : std::as_const(m_entries)) {
        KConfigGroup outputGroup(&tilingGroup, QStringLiteral("Output %1").arg(entry->name()));
        outputGroup.writeEntry("GapLeft", entry->gapLeft());
        outputGroup.writeEntry("GapRight", entry->gapRight());
        outputGroup.writeEntry("GapTop", entry->gapTop());
        outputGroup.writeEntry("GapBottom", entry->gapBottom());
        outputGroup.writeEntry("GapBetween", entry->gapBetween());
        outputGroup.writeEntry("DefaultLayout", entry->defaultLayout());
        outputGroup.writeEntry("MasterRatio", entry->masterRatio());
        outputGroup.writeEntry("MasterCount", entry->masterCount());
        outputGroup.writeEntry("DefaultColumnWidth", entry->defaultColumnWidth());
    }

    setModified(false);
}

void OutputGapOverridesModel::addMonitor(const QString &name, TilingSettings *settings)
{
    if (name.isEmpty() || entryForName(name)) {
        return;
    }

    QString description = name;
    if (qApp) {
        for (const QScreen *screen : qApp->screens()) {
            if (screen && screen->name() == name) {
                description = screenDescription(screen);
                break;
            }
        }
    }

    // Prefill the new override with the current defaults so it starts as a copy.
    const int left = settings ? settings->gapLeft() : kDefaultGapLeft;
    const int right = settings ? settings->gapRight() : kDefaultGapRight;
    const int top = settings ? settings->gapTop() : kDefaultGapTop;
    const int bottom = settings ? settings->gapBottom() : kDefaultGapBottom;
    const int between = settings ? settings->gapBetween() : kDefaultGapBetween;
    const QString layout = settings ? settings->defaultLayout() : QStringLiteral("MasterStack");
    const qreal masterRatio = settings ? settings->masterRatio() : 0.5;
    const int masterCount = settings ? settings->masterCount() : 1;
    const qreal columnWidth = settings ? settings->defaultColumnWidth() : 0.5;

    beginInsertRows({}, m_entries.size(), m_entries.size());
    addEntry(name, description, left, right, top, bottom, between, layout,
             masterRatio, masterCount, columnWidth);
    endInsertRows();
    Q_EMIT countChanged();
    Q_EMIT availableMonitorsChanged();
    setModified(true);
}

void OutputGapOverridesModel::removeMonitor(int row)
{
    if (row < 0 || row >= m_entries.size()) {
        return;
    }
    beginRemoveRows({}, row, row);
    delete m_entries.takeAt(row);
    endRemoveRows();
    Q_EMIT countChanged();
    Q_EMIT availableMonitorsChanged();
    setModified(true);
}

void OutputGapOverridesModel::clearAll()
{
    if (m_entries.isEmpty()) {
        return;
    }
    beginResetModel();
    qDeleteAll(m_entries);
    m_entries.clear();
    endResetModel();
    Q_EMIT countChanged();
    Q_EMIT availableMonitorsChanged();
    setModified(true);
}

TilingRule::TilingRule(QString field, QString pattern, QString action, QObject *parent)
    : QObject(parent)
    , m_field(std::move(field))
    , m_pattern(std::move(pattern))
    , m_action(std::move(action))
{
}

void TilingRule::setField(const QString &value)
{
    if (m_field == value) {
        return;
    }
    m_field = value;
    Q_EMIT fieldChanged();
    Q_EMIT modified();
}

void TilingRule::setPattern(const QString &value)
{
    if (m_pattern == value) {
        return;
    }
    m_pattern = value;
    Q_EMIT patternChanged();
    Q_EMIT modified();
}

void TilingRule::setAction(const QString &value)
{
    if (m_action == value) {
        return;
    }
    m_action = value;
    Q_EMIT actionChanged();
    Q_EMIT modified();
}

TilingRulesModel::TilingRulesModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

TilingRulesModel::~TilingRulesModel()
{
    qDeleteAll(m_entries);
}

QHash<int, QByteArray> TilingRulesModel::roleNames() const
{
    return {
        {FieldRole, "field"},
        {PatternRole, "pattern"},
        {ActionRole, "action"},
        {EntryRole, "entry"},
    };
}

QVariant TilingRulesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }
    TilingRule *entry = m_entries.at(index.row());
    switch (role) {
    case FieldRole:
        return entry->field();
    case PatternRole:
        return entry->pattern();
    case ActionRole:
        return entry->action();
    case EntryRole:
        return QVariant::fromValue(entry);
    }
    return {};
}

int TilingRulesModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_entries.size();
}

void TilingRulesModel::appendEntry(const QString &field, const QString &pattern, const QString &action)
{
    auto *entry = new TilingRule(field, pattern, action, this);
    connect(entry, &TilingRule::modified, this, [this]() {
        setModified(true);
    });
    m_entries.append(entry);
}

void TilingRulesModel::load(const KConfigGroup &rulesGroup)
{
    beginResetModel();
    qDeleteAll(m_entries);
    m_entries.clear();

    const auto loadList = [this, &rulesGroup](const QString &key, const QString &field, const QString &action) {
        const QStringList list = rulesGroup.readEntry(key, QStringList());
        for (const QString &pattern : list) {
            const QString trimmed = pattern.trimmed();
            if (!trimmed.isEmpty()) {
                appendEntry(field, trimmed, action);
            }
        }
    };
    loadList(QStringLiteral("FloatingClass"), QStringLiteral("class"), QStringLiteral("float"));
    loadList(QStringLiteral("FloatingTitle"), QStringLiteral("title"), QStringLiteral("float"));
    loadList(QStringLiteral("IgnoreClass"), QStringLiteral("class"), QStringLiteral("ignore"));
    loadList(QStringLiteral("IgnoreTitle"), QStringLiteral("title"), QStringLiteral("ignore"));

    endResetModel();
    Q_EMIT countChanged();
    setModified(false);
}

void TilingRulesModel::save(KConfigGroup &rulesGroup)
{
    QStringList floatClass;
    QStringList floatTitle;
    QStringList ignoreClass;
    QStringList ignoreTitle;
    for (TilingRule *entry : std::as_const(m_entries)) {
        const QString pattern = entry->pattern().trimmed();
        if (pattern.isEmpty()) {
            continue; // drop blank rows instead of persisting an empty pattern that matches everything
        }
        const bool ignore = entry->action() == QLatin1String("ignore");
        const bool title = entry->field() == QLatin1String("title");
        if (ignore) {
            (title ? ignoreTitle : ignoreClass) << pattern;
        } else {
            (title ? floatTitle : floatClass) << pattern;
        }
    }

    const auto writeOrClear = [&rulesGroup](const QString &key, const QStringList &values) {
        if (values.isEmpty()) {
            rulesGroup.deleteEntry(key);
        } else {
            rulesGroup.writeEntry(key, values);
        }
    };
    writeOrClear(QStringLiteral("FloatingClass"), floatClass);
    writeOrClear(QStringLiteral("FloatingTitle"), floatTitle);
    writeOrClear(QStringLiteral("IgnoreClass"), ignoreClass);
    writeOrClear(QStringLiteral("IgnoreTitle"), ignoreTitle);

    setModified(false);
}

void TilingRulesModel::addRule(const QString &field, const QString &pattern, const QString &action)
{
    beginInsertRows({}, m_entries.size(), m_entries.size());
    appendEntry(field, pattern, action);
    endInsertRows();
    Q_EMIT countChanged();
    setModified(true);
}

void TilingRulesModel::removeRule(int row)
{
    if (row < 0 || row >= m_entries.size()) {
        return;
    }
    beginRemoveRows({}, row, row);
    delete m_entries.takeAt(row);
    endRemoveRows();
    Q_EMIT countChanged();
    setModified(true);
}

void TilingRulesModel::clear()
{
    if (m_entries.isEmpty()) {
        return;
    }
    beginResetModel();
    qDeleteAll(m_entries);
    m_entries.clear();
    endResetModel();
    Q_EMIT countChanged();
    setModified(true);
}

void TilingRulesModel::setModified(bool modified)
{
    if (m_modified == modified) {
        return;
    }
    m_modified = modified;
    Q_EMIT modifiedChanged();
}

DesktopOutputLayoutOverride::DesktopOutputLayoutOverride(uint desktopNumber, QString desktopName,
                                                         int outputIndex, QString outputName, QString outputDescription,
                                                         QString defaultLayout,
                                                         QObject *parent)
    : QObject(parent)
    , m_desktopNumber(desktopNumber)
    , m_desktopName(std::move(desktopName))
    , m_outputIndex(outputIndex)
    , m_outputName(std::move(outputName))
    , m_outputDescription(std::move(outputDescription))
    , m_defaultLayout(std::move(defaultLayout))
{
}

void DesktopOutputLayoutOverride::setDefaultLayout(const QString &value)
{
    m_defaultLayout = value;
    Q_EMIT defaultLayoutChanged();
    Q_EMIT modified();
}

DesktopOutputLayoutOverridesModel::DesktopOutputLayoutOverridesModel(QObject *parent)
    : QAbstractListModel(parent)
{
    if (qApp) {
        connect(qApp, &QGuiApplication::screenAdded, this, [this]() {
            rebuildModel(nullptr, nullptr);
        });
        connect(qApp, &QGuiApplication::screenRemoved, this, [this]() {
            rebuildModel(nullptr, nullptr);
        });
    }
}

DesktopOutputLayoutOverridesModel::~DesktopOutputLayoutOverridesModel()
{
    qDeleteAll(m_entries);
}

QHash<int, QByteArray> DesktopOutputLayoutOverridesModel::roleNames() const
{
    return {
        {DesktopNumberRole, "desktopNumber"},
        {DesktopNameRole, "desktopName"},
        {OutputIndexRole, "outputIndex"},
        {OutputNameRole, "outputName"},
        {OutputDescriptionRole, "outputDescription"},
        {DefaultLayoutRole, "defaultLayout"},
        {EntryRole, "entry"},
    };
}

QVariant DesktopOutputLayoutOverridesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }
    DesktopOutputLayoutOverride *entry = m_entries.at(index.row());
    switch (role) {
    case DesktopNumberRole:
        return entry->desktopNumber();
    case DesktopNameRole:
        return entry->desktopName();
    case OutputIndexRole:
        return entry->outputIndex();
    case OutputNameRole:
        return entry->outputName();
    case OutputDescriptionRole:
        return entry->outputDescription();
    case DefaultLayoutRole:
        return entry->defaultLayout();
    case EntryRole:
        return QVariant::fromValue(entry);
    }
    return {};
}

int DesktopOutputLayoutOverridesModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_entries.size();
}

DesktopOutputLayoutOverride *DesktopOutputLayoutOverridesModel::entryAt(int row) const
{
    if (row < 0 || row >= m_entries.size()) {
        return nullptr;
    }
    return m_entries.at(row);
}

void DesktopOutputLayoutOverridesModel::setModified(bool modified)
{
    if (m_modified == modified) {
        return;
    }
    m_modified = modified;
    Q_EMIT modifiedChanged();
}

void DesktopOutputLayoutOverridesModel::syncOutputList()
{
    QStringList names;
    QStringList descriptions;
    if (qApp) {
        for (const QScreen *screen : qApp->screens()) {
            if (!screen) {
                continue;
            }
            names << screen->name();
            QString desc = screen->model();
            if (desc.isEmpty()) {
                desc = QStringLiteral("%1 %2").arg(screen->manufacturer(), screen->name());
            }
            if (desc.trimmed().isEmpty()) {
                desc = screen->name();
            }
            descriptions << desc;
        }
    }
    if (names != m_outputNames) {
        m_outputNames = names;
        m_outputDescriptions = descriptions;
        Q_EMIT outputNamesChanged();
    }
}

void DesktopOutputLayoutOverridesModel::rebuildModel(const TilingSettings *settings, KConfigGroup *tilingGroup)
{
    Q_UNUSED(settings)

    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("kwinrc"));
    KConfigGroup dtopGroup(config, QStringLiteral("Desktops"));
    const int n = dtopGroup.readEntry("Number", 1);

    syncOutputList();

    QList<uint> newDesktopNums;
    for (int i = 1; i <= n; ++i) {
        newDesktopNums << static_cast<uint>(i);
    }
    if (newDesktopNums == m_desktopNumbers && tilingGroup == nullptr) {
        return;
    }

    beginResetModel();
    qDeleteAll(m_entries);
    m_entries.clear();
    m_desktopNumbers = newDesktopNums;

    for (uint num : std::as_const(m_desktopNumbers)) {
        const QString dName = dtopGroup.readEntry(QStringLiteral("Name_%1").arg(num), i18n("Desktop %1", num));
        for (int oi = 0; oi < m_outputNames.size(); ++oi) {
            const QString &oName = m_outputNames[oi];
            const QString &oDesc = m_outputDescriptions[oi];
            QString entryLayout;
            if (tilingGroup) {
                KConfigGroup perPairGroup(tilingGroup, QStringLiteral("DesktopOutput %1:%2").arg(num).arg(oName));
                if (perPairGroup.hasKey("DefaultLayout")) {
                    entryLayout = perPairGroup.readEntry("DefaultLayout", QString());
                }
            }
            auto *entry = new DesktopOutputLayoutOverride(num, dName, oi, oName, oDesc, entryLayout, this);
            connect(entry, &DesktopOutputLayoutOverride::modified, this, [this]() {
                if (!m_modified) {
                    setModified(true);
                }
            });
            m_entries.append(entry);
        }
    }

    endResetModel();
    Q_EMIT countChanged();
}

void DesktopOutputLayoutOverridesModel::load(KConfigGroup &tilingGroup, const TilingSettings *settings)
{
    rebuildModel(settings, &tilingGroup);
    setModified(false);
}

void DesktopOutputLayoutOverridesModel::refreshFromDefaults(const TilingSettings *settings)
{
    Q_UNUSED(settings)
    beginResetModel();
    for (DesktopOutputLayoutOverride *entry : std::as_const(m_entries)) {
        entry->setDefaultLayout(QString());
    }
    endResetModel();
}

void DesktopOutputLayoutOverridesModel::save(KConfigGroup &tilingGroup, const TilingSettings *settings)
{
    const QString defaultLayout = settings ? settings->defaultLayout() : QStringLiteral("MasterStack");

    const QStringList existing = tilingGroup.groupList();
    for (const QString &sub : existing) {
        if (sub.startsWith(QLatin1String("DesktopOutput "))) {
            tilingGroup.deleteGroup(sub);
        }
    }

    for (DesktopOutputLayoutOverride *entry : std::as_const(m_entries)) {
        const QString layout = entry->defaultLayout();
        if (layout.isEmpty() || layout == defaultLayout) {
            continue;
        }
        KConfigGroup perPairGroup(&tilingGroup,
                                  QStringLiteral("DesktopOutput %1:%2").arg(entry->desktopNumber()).arg(entry->outputName()));
        perPairGroup.writeEntry("DefaultLayout", layout);
    }

    setModified(false);
}

void DesktopOutputLayoutOverridesModel::defaults(const TilingSettings *settings)
{
    Q_UNUSED(settings)
    beginResetModel();
    for (DesktopOutputLayoutOverride *entry : std::as_const(m_entries)) {
        entry->setDefaultLayout(QString());
    }
    endResetModel();
    setModified(false);
}

bool DesktopOutputLayoutOverridesModel::isDefaults(const TilingSettings *settings) const
{
    Q_UNUSED(settings)
    for (DesktopOutputLayoutOverride *entry : m_entries) {
        if (!entry->defaultLayout().isEmpty()) {
            return false;
        }
    }
    return true;
}

TilingKCM::TilingKCM(QObject *parent, const KPluginMetaData &metaData)
    : KQuickManagedConfigModule(parent, metaData)
    , m_settings(new TilingSettings(this))
    , m_gapOverridesModel(new OutputGapOverridesModel(this))
    , m_desktopLayoutOverridesModel(new DesktopOutputLayoutOverridesModel(this))
    , m_rulesModel(new TilingRulesModel(this))
{
    registerSettings(m_settings);
    qmlRegisterAnonymousType<TilingSettings>("org.kde.kwin.kcm.tiling", 1);
    qmlRegisterAnonymousType<OutputGapOverride>("org.kde.kwin.kcm.tiling", 1);
    qmlRegisterAnonymousType<OutputGapOverridesModel>("org.kde.kwin.kcm.tiling", 1);
    qmlRegisterAnonymousType<DesktopOutputLayoutOverride>("org.kde.kwin.kcm.tiling", 1);
    qmlRegisterAnonymousType<DesktopOutputLayoutOverridesModel>("org.kde.kwin.kcm.tiling", 1);
    qmlRegisterAnonymousType<TilingRule>("org.kde.kwin.kcm.tiling", 1);
    qmlRegisterAnonymousType<TilingRulesModel>("org.kde.kwin.kcm.tiling", 1);

    // The base class's settingsChanged() only re-checks the registered
    // KCoreConfigSkeleton objects; it does not consult the virtual
    // isSaveNeeded() override once a skeleton is present. Lift the
    // hand-rolled models' modified state into needsSave ourselves so the
    // Apply button in the KCM shell lights up.
    connect(m_gapOverridesModel, &OutputGapOverridesModel::modifiedChanged, this, &TilingKCM::updateNeedsSave);
    connect(m_desktopLayoutOverridesModel, &DesktopOutputLayoutOverridesModel::modifiedChanged, this, &TilingKCM::updateNeedsSave);
    connect(m_rulesModel, &TilingRulesModel::modifiedChanged, this, &TilingKCM::updateNeedsSave);

    connect(m_settings, &TilingSettings::defaultLayoutChanged, this, [this]() {
        m_desktopLayoutOverridesModel->refreshFromDefaults(m_settings);
    });
    connect(m_settings, &TilingSettings::enabledLayoutsChanged, this, [this]() {
        m_desktopLayoutOverridesModel->refreshFromDefaults(m_settings);
    });
}

TilingKCM::~TilingKCM() = default;

TilingSettings *TilingKCM::settings() const
{
    return m_settings;
}

OutputGapOverridesModel *TilingKCM::gapOverridesModel() const
{
    return m_gapOverridesModel;
}

DesktopOutputLayoutOverridesModel *TilingKCM::desktopLayoutOverridesModel() const
{
    return m_desktopLayoutOverridesModel;
}

TilingRulesModel *TilingKCM::rulesModel() const
{
    return m_rulesModel;
}

void TilingKCM::updateNeedsSave()
{
    const bool modelSaveNeeded = m_gapOverridesModel->isModified()
        || m_desktopLayoutOverridesModel->isModified()
        || m_rulesModel->isModified();
    setNeedsSave(m_settings->isSaveNeeded() || modelSaveNeeded);
    if (modelSaveNeeded) {
        setRepresentsDefaults(false);
    }
}

void TilingKCM::pickWindow()
{
    // queryWindowInfo is a delayed reply: KWin turns the cursor into a
    // targeting reticule and answers once the user clicks a window (or errors
    // if they cancel). Same call the native Window Rules "Detect" button uses.
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
                                                          QStringLiteral("/KWin"),
                                                          QStringLiteral("org.kde.KWin"),
                                                          QStringLiteral("queryWindowInfo"));
    constexpr int kPickTimeoutMs = 5 * 60 * 1000; // long: waits on user interaction
    QDBusPendingCall pending = QDBusConnection::sessionBus().asyncCall(message, kPickTimeoutMs);
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *self) {
        self->deleteLater();
        const QDBusPendingReply<QVariantMap> reply = *self;
        if (reply.isError()) {
            return; // user cancelled the pick (Esc) or no window was selected
        }
        const QVariantMap info = reply.value();
        Q_EMIT windowPicked(info.value(QStringLiteral("resourceClass")).toString(),
                            info.value(QStringLiteral("caption")).toString());
    });
}

void TilingKCM::load()
{
    KQuickManagedConfigModule::load();

    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("kwinrc"));
    KConfigGroup tilingGroup(config, QStringLiteral("Tiling"));
    m_gapOverridesModel->load(tilingGroup, m_settings);
    m_desktopLayoutOverridesModel->load(tilingGroup, m_settings);

    KConfigGroup rulesGroup(config, QStringLiteral("TilingRules"));
    m_rulesModel->load(rulesGroup);
}

void TilingKCM::save()
{
    KQuickManagedConfigModule::save();

    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("kwinrc"));
    KConfigGroup tilingGroup(config, QStringLiteral("Tiling"));
    m_gapOverridesModel->save(tilingGroup, m_settings);
    m_desktopLayoutOverridesModel->save(tilingGroup, m_settings);

    KConfigGroup rulesGroup(config, QStringLiteral("TilingRules"));
    m_rulesModel->save(rulesGroup);

    config->sync();

    QDBusMessage message = QDBusMessage::createSignal(QStringLiteral("/KWin"),
                                                       QStringLiteral("org.kde.KWin"),
                                                       QStringLiteral("reloadConfig"));
    QDBusConnection::sessionBus().send(message);
}

void TilingKCM::defaults()
{
    KQuickManagedConfigModule::defaults();
    m_gapOverridesModel->clearAll();
    m_desktopLayoutOverridesModel->defaults(m_settings);
    m_rulesModel->clear();
}

bool TilingKCM::isSaveNeeded() const
{
    if (m_settings->isSaveNeeded()) {
        return true;
    }
    return m_gapOverridesModel->isModified()
        || m_desktopLayoutOverridesModel->isModified()
        || m_rulesModel->isModified();
}

bool TilingKCM::isDefaults() const
{
    if (!m_settings->isDefaults()) {
        return false;
    }
    if (!m_rulesModel->isEmpty()) {
        return false;
    }
    return m_gapOverridesModel->isDefaults() && m_desktopLayoutOverridesModel->isDefaults(m_settings);
}

} // namespace KWin

#include "tilingkcm.moc"
#include "moc_tilingkcm.cpp"
