# Fork of peterfajdiga's kwin4_effect_geometry_change, tuned to consume native
# kwin-tiling reflow hints. JS KWin effect — not a compositor fork.
{
  lib,
  stdenvNoCC,
}:

stdenvNoCC.mkDerivation {
  pname = "kwin-effects-tiling-reflow";
  version = "1.0.0";

  src = ./.;

  dontBuild = true;

  installPhase = ''
    runHook preInstall

    # KWin scripted effects load a single MainScript; prepend the shared helper.
    cat lib/reflowanimation.js package/contents/code/main.js > package/contents/code/main.js.combined
    mv package/contents/code/main.js.combined package/contents/code/main.js

    mkdir -p $out/share/kwin/effects/kwin4_effect_tiling_reflow
    cp -a package/. $out/share/kwin/effects/kwin4_effect_tiling_reflow/

    runHook postInstall
  '';

  doInstallCheck = true;
  installCheckPhase = ''
    runHook preInstallCheck
    test -f $out/share/kwin/effects/kwin4_effect_tiling_reflow/metadata.json
    grep -q TilingReflowAnimation $out/share/kwin/effects/kwin4_effect_tiling_reflow/contents/code/main.js
    grep -q WindowTilingReflowRole $out/share/kwin/effects/kwin4_effect_tiling_reflow/contents/code/main.js
    grep -q '"Id": "kwin4_effect_tiling_reflow"' $out/share/kwin/effects/kwin4_effect_tiling_reflow/metadata.json
    runHook postInstallCheck
  '';

  passthru = {
    pluginId = "kwin4_effect_tiling_reflow";
  };

  meta = {
    description = "KWin effect: animate tiled reflow using WindowTilingReflowRole hints";
    homepage = "https://github.com/luxus/kwin-tiling";
    license = lib.licenses.gpl3Only;
    platforms = lib.platforms.linux;
  };
})
