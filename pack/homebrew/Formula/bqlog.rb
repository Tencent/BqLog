class Bqlog < Formula
  desc "Lightweight, high-performance, cross-language logging library"
  homepage "https://github.com/Tencent/BqLog"
  url "https://github.com/Tencent/BqLog/archive/refs/tags/Release_2.7.0.tar.gz"
  sha256 "aed63a3e5b38b35bdb10d6f1eef134e727af4b5e63ead165c5817f4271dd3517"
  license "Apache-2.0"

  head "https://github.com/Tencent/BqLog.git", branch: "develop"

  # The release tag is Release_<version>, which is not the v<version> livecheck assumes.
  livecheck do
    url :stable
    regex(/^Release[._-]v?(\d+(?:\.\d+)+)$/i)
  end

  depends_on "cmake" => :build

  def install
    # Both library forms and the command line tools, so a single install covers every use.
    args = ["-DBUILD_LIB_TYPE=both", "-DBQ_BUILD_TOOLS=ON", "-DBQ_WERROR=OFF"]
    # A package manager installs for the native architecture, not a universal binary.
    args << "-DCMAKE_OSX_ARCHITECTURES=#{Hardware::CPU.arch}" if OS.mac?

    system "cmake", "-S", ".", "-B", "build", *std_cmake_args, *args
    system "cmake", "--build", "build", "--parallel"
    system "cmake", "--install", "build"
  end

  test do
    (testpath/"test.cpp").write <<~CPP
      #include <bq_log/bq_log.h>

      int main() {
          bq::log log = bq::log::create_log("homebrew_test", "");
          log.info("bqlog installed and linked");
          return 0;
      }
    CPP
    system ENV.cxx, "test.cpp", "-std=c++17",
           "-I#{include}", "-L#{lib}", "-lBqLog", "-Wl,-rpath,#{lib}",
           "-o", "test"
    system "./test"

    assert_predicate bin/"BqLog_CategoryLogGenerator", :executable?
    assert_predicate bin/"BqLog_LogDecoder", :executable?
  end
end
