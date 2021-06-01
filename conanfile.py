from conans import ConanFile, CMake, tools
import re
from os import path

class ConcoreRecipe(ConanFile):
   name = "concore"
   description = "Core abstractions for dealing with concurrency in C++"
   author = "Lucian Radu Teodorescu"
   topics = ("concurrency", "tasks", "executors", "no-locks")
   homepage = "https://github.com/lucteo/concore"
   url = "https://github.com/lucteo/concore"
   license = "MIT"

   settings = "os", "compiler", "build_type", "arch"
   generators = "cmake"
   build_policy = "missing"   # Some of the dependencies don't have builds for all our targets

   options = {"shared": [True, False], "fPIC": [True, False]}
   default_options = {"shared": False, "fPIC": True, "catch2:with_main": True}

   exports = "LICENSE"
   exports_sources = ("src/*", "include/*", "CMakeLists.txt")

   def set_version(self):
      # Get the version from src/CMakeList.txt project definition
      content = tools.load(path.join(self.recipe_folder, "src/CMakeLists.txt"))
      version = re.search(r"project\([^\)]+VERSION (\d+\.\d+\.\d+)[^\)]*\)", content).group(1)
      self.version = version.strip()

   @property
   def _run_tests(self):
       return tools.get_env("CONAN_RUN_TESTS", False)

   def build_requirements(self):
      if self._run_tests:
         self.build_requires("catch2/2.13.6")
         self.build_requires("rapidcheck/20210107")
         self.build_requires("benchmark/1.5.3")

   def config_options(self):
       if self.settings.os == "Windows":
           del self.options.fPIC

   def configure(self):
      # TODO: do we want to provide a header+inline source version of the library? If yes, this is the place to do it
      # TODO: shared vs static library
      pass

   def build(self):
      # Note: options "shared" and "fPIC" are automatically handled in CMake
      cmake = self._configure_cmake()
      cmake.build()

   def package(self):
      self.copy(pattern="LICENSE", dst="licenses")
      cmake = self._configure_cmake()
      cmake.install()

   def package_info(self):
      self.cpp_info.libs = self.collect_libs()

   def _configure_cmake(self):
      cmake = CMake(self)
      cmake.configure(source_folder=None if self._run_tests else "src")
      return cmake


