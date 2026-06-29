/*
    KWin - the KDE window manager
    SPDX-FileCopyrightText: 2026 KWin Tiling Fork
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KQuickManagedConfigModule>

#include <QAbstractListModel>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVector>

class KConfigGroup;
class QScreen;
class TilingSettings;

namespace KWin
{

/**
 * Per-output gap/layout override entry, exposed to QML.
 *
 * One entry exists for each monitor the user has explicitly customized (an
 * "Output <name>" sub-group in kwinrc). Monitors without an entry follow the
 * global defaults. Editing the values writes them into the per-output sub-group
 * so the tiling controller can read them back at runtime.
 */
class OutputGapOverride : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString description READ description CONSTANT)
    Q_PROPERTY(int gapLeft READ gapLeft WRITE setGapLeft NOTIFY gapLeftChanged)
    Q_PROPERTY(int gapRight READ gapRight WRITE setGapRight NOTIFY gapRightChanged)
    Q_PROPERTY(int gapTop READ gapTop WRITE setGapTop NOTIFY gapTopChanged)
    Q_PROPERTY(int gapBottom READ gapBottom WRITE setGapBottom NOTIFY gapBottomChanged)
    Q_PROPERTY(int gapBetween READ gapBetween WRITE setGapBetween NOTIFY gapBetweenChanged)
    Q_PROPERTY(QString defaultLayout READ defaultLayout WRITE setDefaultLayout NOTIFY defaultLayoutChanged)

public:
    explicit OutputGapOverride(QString name, QString description, int gapLeft, int gapRight,
                               int gapTop, int gapBottom, int gapBetween, QString defaultLayout,
                               QObject *parent = nullptr);

    QString name() const { return m_name; }
    QString description() const { return m_description; }

    int gapLeft() const { return m_gapLeft; }
    int gapRight() const { return m_gapRight; }
    int gapTop() const { return m_gapTop; }
    int gapBottom() const { return m_gapBottom; }
    int gapBetween() const { return m_gapBetween; }
    QString defaultLayout() const { return m_defaultLayout; }

    void setGapLeft(int value);
    void setGapRight(int value);
    void setGapTop(int value);
    void setGapBottom(int value);
    void setGapBetween(int value);
    void setDefaultLayout(const QString &value);

Q_SIGNALS:
    void gapLeftChanged();
    void gapRightChanged();
    void gapTopChanged();
    void gapBottomChanged();
    void gapBetweenChanged();
    void defaultLayoutChanged();

    /**
     * Emitted whenever any of the gap values change. The owning model
     * uses this to mark itself as modified.
     */
    void modified();

private:
    QString m_name;
    QString m_description;
    int m_gapLeft;
    int m_gapRight;
    int m_gapTop;
    int m_gapBottom;
    int m_gapBetween;
    QString m_defaultLayout;
};

/**
 * List model of per-output overrides backed by kwinrc.
 *
 * Holds one entry per customized monitor (an "Output <name>" sub-group under
 * [Tiling]). Monitors without an entry follow the global defaults, so the model
 * starts empty and the user adds overrides on demand from availableMonitors().
 * Edits are kept in memory until TilingKCM::save() writes them back.
 */
class OutputGapOverridesModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    // Connected monitors without a custom override, as {name, description} maps
    // for the "add custom settings" dropdown.
    Q_PROPERTY(QVariantList availableMonitors READ availableMonitors NOTIFY availableMonitorsChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        DescriptionRole,
        GapLeftRole,
        GapRightRole,
        GapTopRole,
        GapBottomRole,
        GapBetweenRole,
        DefaultLayoutRole,
        EntryRole,
    };
    Q_ENUM(Roles)

    explicit OutputGapOverridesModel(QObject *parent = nullptr);
    ~OutputGapOverridesModel() override;

    QHash<int, QByteArray> roleNames() const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariantList availableMonitors() const;

    /**
     * Re-populate the model from the per-output override sub-groups in the
     * supplied Tiling group (one entry per "Output <name>" sub-group).
     */
    void load(KConfigGroup &tilingGroup, const TilingSettings *settings);

    /**
     * Replace the per-output sub-groups in the Tiling group with one sub-group
     * per customized monitor (all values, so the customization persists).
     */
    void save(KConfigGroup &tilingGroup, const TilingSettings *settings);

    /**
     * Add a custom override for `name`, prefilled with the current defaults.
     */
    Q_INVOKABLE void addMonitor(const QString &name, TilingSettings *settings);

    /**
     * Remove the override at `row`; that monitor falls back to the defaults.
     */
    Q_INVOKABLE void removeMonitor(int row);

    /**
     * Remove every override (the "reset" button and the KCM Defaults action) so
     * all monitors follow the defaults again.
     */
    Q_INVOKABLE void clearAll();

    /**
     * Returns true if any entry differs from its loaded state. Used by the
     * KCM to drive the Apply button.
     */
    bool isModified() const { return m_modified; }

    /**
     * Default state = no per-monitor overrides. Used to drive the KCM Defaults
     * button.
     */
    bool isDefaults() const { return m_entries.isEmpty(); }

Q_SIGNALS:
    void countChanged();
    void modifiedChanged();
    void availableMonitorsChanged();

private:
    void addEntry(const QString &name, const QString &description, int left, int right,
                  int top, int bottom, int between, const QString &defaultLayout);
    OutputGapOverride *entryForName(const QString &name) const;
    void setModified(bool modified);

    QVector<OutputGapOverride *> m_entries;
    bool m_modified = false;
};

/**
 * A single float/ignore rule, exposed to QML.
 *
 * Each rule matches windows by either their application class or their window
 * title (caption), and either floats them (kept tiling-aware) or ignores them
 * entirely. The four backing kwinrc keys (FloatingClass/FloatingTitle/
 * IgnoreClass/IgnoreTitle in [TilingRules]) are flattened into a single list of
 * these so the KCM can present one rule table.
 */
class TilingRule : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString field READ field WRITE setField NOTIFY fieldChanged)
    Q_PROPERTY(QString pattern READ pattern WRITE setPattern NOTIFY patternChanged)
    Q_PROPERTY(QString action READ action WRITE setAction NOTIFY actionChanged)

public:
    explicit TilingRule(QString field, QString pattern, QString action, QObject *parent = nullptr);

    QString field() const { return m_field; }    // "class" or "title"
    QString pattern() const { return m_pattern; }
    QString action() const { return m_action; }   // "float" or "ignore"

    void setField(const QString &value);
    void setPattern(const QString &value);
    void setAction(const QString &value);

Q_SIGNALS:
    void fieldChanged();
    void patternChanged();
    void actionChanged();
    void modified();

private:
    QString m_field;
    QString m_pattern;
    QString m_action;
};

/**
 * List model of float/ignore rules backed by the [TilingRules] group in kwinrc.
 *
 * Loads the four StringList keys into a flat list of TilingRule rows and writes
 * them back out on save, grouped by (field, action). The tiling controller
 * reads the same keys directly (see TilingRules::load), so saving + a config
 * reload applies the rules live.
 */
class TilingRulesModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        FieldRole = Qt::UserRole + 1,
        PatternRole,
        ActionRole,
        EntryRole,
    };
    Q_ENUM(Roles)

    explicit TilingRulesModel(QObject *parent = nullptr);
    ~TilingRulesModel() override;

    QHash<int, QByteArray> roleNames() const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    void load(const KConfigGroup &rulesGroup);
    void save(KConfigGroup &rulesGroup);

    Q_INVOKABLE void addRule(const QString &field, const QString &pattern, const QString &action);
    Q_INVOKABLE void removeRule(int row);
    Q_INVOKABLE void clear();

    bool isModified() const { return m_modified; }
    bool isEmpty() const { return m_entries.isEmpty(); }

Q_SIGNALS:
    void countChanged();
    void modifiedChanged();

private:
    void appendEntry(const QString &field, const QString &pattern, const QString &action);
    void setModified(bool modified);

    QVector<TilingRule *> m_entries;
    bool m_modified = false;
};

/**
 * Tiling settings KCM module.
 *
 * Hosts the default settings (via TilingSettings) and a model of
 * per-output gap overrides. Saving the module writes both the default
 * settings and the per-output override sub-groups to kwinrc, then
 * notifies KWin via DBus to reload the configuration.
 */
class TilingKCM : public KQuickManagedConfigModule
{
    Q_OBJECT
    Q_PROPERTY(TilingSettings *settings READ settings CONSTANT)
    Q_PROPERTY(KWin::OutputGapOverridesModel *gapOverridesModel READ gapOverridesModel CONSTANT)
    Q_PROPERTY(KWin::TilingRulesModel *rulesModel READ rulesModel CONSTANT)

public:
    explicit TilingKCM(QObject *parent, const KPluginMetaData &metaData);
    ~TilingKCM() override;

    TilingSettings *settings() const;
    OutputGapOverridesModel *gapOverridesModel() const;
    TilingRulesModel *rulesModel() const;

    bool isSaveNeeded() const override;
    bool isDefaults() const override;

    /**
     * Ask the running KWin to let the user point at a window (the same
     * targeting reticule the native Window Rules editor uses, via the
     * org.kde.KWin queryWindowInfo DBus call) and emit windowPicked() with
     * its class and caption. No-op/silent if the user cancels.
     */
    Q_INVOKABLE void pickWindow();

Q_SIGNALS:
    void windowPicked(const QString &resourceClass, const QString &caption);

public Q_SLOTS:
    void load() override;
    void save() override;
    void defaults() override;

private:
    void updateNeedsSave();

    TilingSettings *m_settings;
    OutputGapOverridesModel *m_gapOverridesModel;
    TilingRulesModel *m_rulesModel;
};

} // namespace KWin
