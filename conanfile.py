from conans import ConanFile, CMake

class ConcoreRecipe(ConanFile):
   name = "concore"
   description = "Core abstractions for dealing with concurrency in C++"
   author = "Lucian Radu Teodorescu"
   version = "0.6.0"
   topics = ("concurrency", "tasks", "executors", "no-locks")
   homepage = "https://github.com/lucteo/concore"
   license = "MIT"

   requires = "catch2/2.13.6", "rapidcheck/20210107", "benchmark/1.5.3"

   settings = "os", "compiler", "build_type", "arch"
   generators = "cmake"
   build_policy = "missing"   # Some of the dependencies don't have builds for all our targets

   options = {"shared": [True, False], "fPIC": [True, False]}
   default_options = {"shared": False, "fPIC": True, "catch2:with_main": True}

   exports = "LICENSE"
   exports_sources = ("src/*", "include/*", "CMakeLists.txt")

   def source(self):
      # TODO: do we want to generate the version file here?
      pass

   def config_options(self):
       if self.settings.os == "Windows":
           del self.options.fPIC

   def configure(self):
      # TODO: do we want to provide a header+inline source version of the library? If yes, this is the place to do it
      pass

   def build(self):
      # Note: options "shared" and "fPIC" are automatically handled in CMake
      cmake = self._configure_cmake()
      cmake.build()

   # TODO: finalize packaging
   def package(self):
      # self.copy("*")
      self.copy("LICENSE")
      self.copy("src/*")
      self.copy("include/*")

   def package_info(self):
      self.cpp_info.libs = self.collect_libs()

   def _configure_cmake(self):
      cmake = CMake(self)
      cmake.definitions["CONCORE_BUILD_TESTS"] = "OFF"
      cmake.definitions["CONCORE_BUILD_EXAMPLES"] = "OFF"
      cmake.definitions["CONCORE_INSTALL_DOCS"] = "OFF"
      cmake.definitions["CONCORE_INSTALL_HELPERS"] = "ON"
      cmake.configure(build_folder="build")
      return cmake


