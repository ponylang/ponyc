{ pkgs, libsSource }:
let
  definitions = builtins.readFile (libsSource + "/lib/CMakeLists.txt");
  revision = builtins.head (builtins.match
    ".*set\\(LLVM_DESIRED_HASH \"([0-9a-f]+)\"\\).*" definitions);
in {
  llvmRevision = revision;
  llvm = pkgs.fetchFromGitHub {
    owner = "llvm";
    repo = "llvm-project";
    rev = revision;
    hash = "sha256-+e5YvYEbCvvEQbLVIQP5hwPp4tEk6EB270bSEMKv0t4=";
  };
  benchmark = pkgs.fetchurl {
    url = "https://github.com/google/benchmark/archive/v1.9.5.tar.gz";
    hash = "sha256-ljE0HIK6xKKIvvlR+LJrQfaQIXlBhOzpafhHOXfqo0A=";
  };
  googletest = pkgs.fetchurl {
    url = "https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz";
    hash = "sha256-Zfq3AdmCnTjLd8FKzcQx0hCL/b+JeeQOuK5Wft8Qsnw=";
  };
}
