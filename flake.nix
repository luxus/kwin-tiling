{
  description = "Native dynamic tiling patched into KWin — package, overlay, and NixOS module";

  # Temporarily tracks nixpkgs master for KDE Frameworks >= 6.30, which KWin 6.8
  # beta (6.7.90) requires and which nixos-unstable has not yet promoted. Revert
  # to nixos-unstable once it ships Frameworks 6.30 (imminent in the 6.8 cycle).
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/master";

  outputs =
    { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});
    in
    {
      # The patched KWin: stock kdePackages.kwin + the tiling hooks.patch + the
      # vendored src/ under pkgs/kwin-tiling. callPackage supplies the stock
      # kdePackages, so this builds without any overlay applied (no recursion).
      packages = forAllSystems (pkgs: rec {
        kwin-tiling = pkgs.callPackage ./pkgs/kwin-tiling { };
        default = kwin-tiling;
      });

      # Drop-in replacement of kdePackages.kwin with the patched build. The patch
      # is built from the pristine prev.kdePackages so it never self-references
      # final.kdePackages.kwin (which would recurse).
      overlays.default = _final: prev: {
        kdePackages = prev.kdePackages // {
          kwin = import ./pkgs/kwin-tiling { inherit (prev) lib kdePackages fetchurl libcap; };
        };
      };

      # Compose onto a host to give it native KWin tiling. Patching kwin rebuilds
      # the compositor and its reverse-deps, so apply it only where you want it
      # (not globally across a fleet).
      nixosModules.kwin-tiling =
        { ... }:
        {
          nixpkgs.overlays = [ self.overlays.default ];
        };

      # Fast, KWin-free self-check of the pure column arithmetic (the part that is
      # easy to get subtly wrong). `nix flake check` runs it without building kwin.
      checks = forAllSystems (pkgs: {
        columnmath =
          pkgs.runCommand "kwin-tiling-columnmath-test" { nativeBuildInputs = [ pkgs.gcc ]; }
            ''
              g++ -std=c++20 -O2 -Wall -Wextra -o columnmath-test \
                ${./pkgs/kwin-tiling}/tests/columnmath_test.cpp
              ./columnmath-test
              touch $out
            '';
        gridmath =
          pkgs.runCommand "kwin-tiling-gridmath-test" { nativeBuildInputs = [ pkgs.gcc ]; }
            ''
              g++ -std=c++20 -O2 -Wall -Wextra -o gridmath-test \
                ${./pkgs/kwin-tiling}/tests/gridmath_test.cpp
              ./gridmath-test
              touch $out
            '';
        directionmath =
          pkgs.runCommand "kwin-tiling-directionmath-test" { nativeBuildInputs = [ pkgs.gcc ]; }
            ''
              g++ -std=c++20 -O2 -Wall -Wextra -o directionmath-test \
                ${./pkgs/kwin-tiling}/tests/directionmath_test.cpp
              ./directionmath-test
              touch $out
            '';
        masterstackmath =
          pkgs.runCommand "kwin-tiling-masterstackmath-test" { nativeBuildInputs = [ pkgs.gcc ]; }
            ''
              g++ -std=c++20 -O2 -Wall -Wextra -o masterstackmath-test \
                ${./pkgs/kwin-tiling}/tests/masterstackmath_test.cpp
              ./masterstackmath-test
              touch $out
            '';
        movestate =
          pkgs.runCommand "kwin-tiling-movestate-test" { nativeBuildInputs = [ pkgs.gcc ]; }
            ''
              g++ -std=c++20 -O2 -Wall -Wextra -o movestate-test \
                ${./pkgs/kwin-tiling}/tests/movestate_test.cpp
              ./movestate-test
              touch $out
            '';
        classmatch =
          pkgs.runCommand "kwin-tiling-classmatch-test" { nativeBuildInputs = [ pkgs.gcc ]; }
            ''
              g++ -std=c++20 -O2 -Wall -Wextra -o classmatch-test \
                ${./pkgs/kwin-tiling}/tests/classmatch_test.cpp
              ./classmatch-test
              touch $out
            '';
        suspendpolicy =
          pkgs.runCommand "kwin-tiling-suspendpolicy-test" { nativeBuildInputs = [ pkgs.gcc ]; }
            ''
              g++ -std=c++20 -O2 -Wall -Wextra -o suspendpolicy-test \
                ${./pkgs/kwin-tiling}/tests/suspendpolicy_test.cpp
              ./suspendpolicy-test
              touch $out
            '';
        sizingpolicy =
          pkgs.runCommand "kwin-tiling-sizingpolicy-test" { nativeBuildInputs = [ pkgs.gcc ]; }
            ''
              g++ -std=c++20 -O2 -Wall -Wextra -o sizingpolicy-test \
                ${./pkgs/kwin-tiling}/tests/sizingpolicy_test.cpp
              ./sizingpolicy-test
              touch $out
            '';
        leafcolumn =
          pkgs.runCommand "kwin-tiling-leafcolumn-test" { nativeBuildInputs = [ pkgs.gcc ]; }
            ''
              g++ -std=c++20 -O2 -Wall -Wextra -o leafcolumn-test \
                ${./pkgs/kwin-tiling}/tests/leafcolumn_test.cpp
              ./leafcolumn-test
              touch $out
            '';
        movefsm =
          pkgs.runCommand "kwin-tiling-movefsm-test" { nativeBuildInputs = [ pkgs.gcc ]; }
            ''
              g++ -std=c++20 -O2 -Wall -Wextra -o movefsm-test \
                ${./pkgs/kwin-tiling}/tests/movefsm_test.cpp
              ./movefsm-test
              touch $out
            '';
        tilingconfig =
          pkgs.runCommand "kwin-tiling-tilingconfig-test" { nativeBuildInputs = [ pkgs.gcc ]; }
            ''
              g++ -std=c++20 -O2 -Wall -Wextra -o tilingconfig-test \
                ${./pkgs/kwin-tiling}/tests/tilingconfig_test.cpp
              ./tilingconfig-test
              touch $out
            '';
        # Single entry that runs the whole pure suite (same as tests/run.sh).
        pure-suite =
          pkgs.runCommand "kwin-tiling-pure-suite" { nativeBuildInputs = [ pkgs.gcc pkgs.bash ]; }
            ''
              cp -r ${./pkgs/kwin-tiling} tree
              chmod -R u+w tree
              bash tree/tests/run.sh
              touch $out
            '';
      });

      formatter = forAllSystems (pkgs: pkgs.nixfmt);
    };
}
