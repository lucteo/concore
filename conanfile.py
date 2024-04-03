from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout, CMakeDeps, CMakeToolchain
from conan.tools.files import copy, load, collect_libs
from conan.tools.microsoft.visual import is_msvc
from os import path
from re import search


class ConcoreRecipe(ConanFile):
   name = "concore"
   description = "Core abstractions for dealing with concurrency in C++"
   author = "Lucian Radu Teodorescu"
   topics = ("concurrency", "tasks", "executors", "no-locks")
   homepage = "https://github.com/lucteo/concore"
   url = "https://github.com/lucteo/concore"
   license = "MIT"

   settings = "os", "compiler", "build_type", "arch"
   build_policy = "missing"   # Some of the dependencies don't have builds for all our targets

   options = {"shared": [True, False], "fPIC": [True, False], "no_tbb": [True, False]}
   default_options = {"shared": False, "fPIC": True, "no_tbb": False, "catch2/*:with_main": True}

   exports = "LICENSE"
   exports_sources = ("src/*", "test/*", "CMakeLists.txt", "LICENSE")

   def set_version(self):
      # Get the version from src/CMakeList.txt project definition
      content = load(self, path.join(self.recipe_folder, "src", "CMakeLists.txt"))
      version = search(r"project\([^\)]+VERSION (\d+\.\d+\.\d+)[^\)]*\)", content).group(1)
      self.version = version.strip()

   @property
   def _run_tests(self):
      return not self.conf.get("tools.build:skip_test", default=True)
   
   @property
   def _src_folder(self):
      return "." if self._run_tests else "src"

   def build_requirements(self):
      if not self.options.no_tbb:
         self.test_requires("onetbb/2021.10.0")

      if self._run_tests:
         self.test_requires("catch2/2.13.7")
         self.test_requires("rapidcheck/cci.20210107")
         self.test_requires("benchmark/1.5.6")

   def config_options(self):
      if self.settings.os == "Windows":
         del self.options.fPIC

   def layout(self):
      cmake_layout(self, src_folder = self._src_folder)

   def generate(self):
      tc = CMakeToolchain(self)
      tc.variables["concore.no_tbb"] = self.options.no_tbb
      if self.settings.os == "Windows" and is_msvc(self) and self.options.shared:
         tc.variables["CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS"] = True
      tc.generate()

      deps = CMakeDeps(self)
      deps.generate()

   def build(self):
      # Note: options "shared" and "fPIC" are automatically handled in CMake
      cmake = self._configure_cmake()
      cmake.build()
      if self._run_tests:
         cmake.test()

   def package(self):
      copy(self, "LICENSE", src=self.source_folder, dst=path.join(self.package_folder, "licenses"))
      cmake = self._configure_cmake()
      cmake.install()

   def package_info(self):
      self.cpp_info.libs = collect_libs(self)

   def package_id(self):
      del self.info.options.no_tbb

   def _configure_cmake(self):
      cmake = CMake(self)
      cmake.configure()
      return cmake
