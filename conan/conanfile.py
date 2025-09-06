from conan import ConanFile, tools
from conan.errors import ConanInvalidConfiguration
from conan.tools.scm import Version
from conan.tools.build import check_min_cppstd
from conan.tools.files import copy, get
from conan.tools.layout import basic_layout
import os

required_conan_version = ">=1.52.0"

class GaiaConan(ConanFile):
    name = "gaia-ecs"
    version = "0.9.2"
    license = "MIT"
    package_type = "header-library"
    description = "A simple and powerful entity component system (ECS) written in C++17"
    homepage = "https://github.com/richardbiely/gaia-ecs"
    url = "https://github.com/conan-io/conan-center-index"
    topics = ("gamedev", "performance", "entity", "ecs")
    exports_sources = "include/*", "LICENSE"
    no_copy_source = True
    settings = "os", "arch", "compiler", "build_type"

    @property
    def _min_cppstd(self):
        return "17"

    @property
    def _compilers_minimum_version(self):
        return {
            "Visual Studio": "16",
            "msvc": "192",
            "gcc": "10",
            "clang": "7.0",
            "apple-clang": "10.0"
        }
    
    def source(self):
        # Dual Conan 1.x / 2.x get()
        try:
            # Conan 2.x
            from conan.tools.files import get as conan_get
            conan_get(self, **self.conan_data["sources"][self.version], strip_root=True)
        except ImportError:
            # Conan 1.x
            from conans import tools
            tools.get(**self.conan_data["sources"][self.version], strip_root=True)

    def package_id(self):
        # Header-only: make package independent of settings
        try:
            self.info.header_only()
        except AttributeError:
            # Conan 1.x old version fallback
            self.info.settings.clear()

    def validate(self):
        if self.settings.compiler.get_safe("cppstd"):
            check_min_cppstd(self, self._min_cppstd)

        minimum_version = self._compilers_minimum_version.get(str(self.settings.compiler), False)
        if minimum_version and Version(self.settings.compiler.version) < minimum_version:
            raise ConanInvalidConfiguration(
                f"{self.ref} requires C++{self._min_cppstd}, which your compiler does not support."
            )

    def package(self):
        copy(self, "LICENSE", self.source_folder, self.package_folder)
        copy(self,
         pattern="*",
         src=os.path.join(self.source_folder, "include"),
         dst=os.path.join(self.package_folder, "include"),
         keep_path=True)

    def package_info(self):
        if self.settings.os in ["FreeBSD", "Linux"]:
            self.cpp_info.system_libs = ["pthread"]

        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
        self.cpp_info.includedirs = ["include"]

        # Conan 1.x old CMake generators
        try:
            self.cpp_info.names["cmake_find_package"] = "gaia"
            self.cpp_info.names["cmake_find_package_multi"] = "gaia"
        except AttributeError:
            pass

        # Conan 2.x / modern generators
        try:
            self.cpp_info.set_property("cmake_file_name", "gaia")
            self.cpp_info.set_property("cmake_target_name", "gaia::gaia")
        except AttributeError:
            pass
        