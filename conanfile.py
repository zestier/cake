from conans import ConanFile, CMake
import os

class CakeConan(ConanFile):
    name = "cake"
    version = "0.1"
    settings = "os", "compiler", "build_type", "arch"
    requires = (
        "bgfx/7188@zstiers-ext/stable",
        "FastNoise/0.4@zstiers-ext/stable",
        "fmt/5.3.0@bincrafters/stable",
        "glm/0.9.9.8@_/_",
        "lime/0.1@zestier/test",
        "sdl2/2.0.12@bincrafters/stable",
        "spdlog/1.3.1@bincrafters/stable",
    )
    exports_sources = (
        "CMakeLists.txt",
        "assets/*",
        "src/*",
    )
    generators = ("cmake")
    keep_imports = True
    default_options = {
        "boost:header_only": True,
    }

    def _configure_cmake(self):
        cmake = CMake(self)
        cmake.configure()
        return cmake

    def build(self):
        cmake = self._configure_cmake()
        cmake.build()

    def install(self):
        cmake = self._configure_cmake()
        cmake.install()

    def configure(self):
        self.options["sdl2"].shared = False
        self.options["sdl2"].iconv = False

    def imports(self):
        dest = os.getenv("CONAN_IMPORT_PATH", "bin")
        self.copy("*.dll", dst=dest, src="bin")
        self.copy("*.dylib*", dst=dest, src="lib")
        self.copy("*.html", dst=os.getcwd(), src="bin")
        self.copy("*.wasm", dst=os.getcwd(), src="bin")
        self.copy("*.js", dst=os.getcwd(), src="bin")
        self.copy("*.bin", dst=os.getcwd(), src="bin")
        
    def package(self):
        self.copy("*", dst="bin", src="bin")
        self.copy("*", dst="bin/assets", src="assets")
