from conans import ConanFile, CMake

class ConcoreRecipe(ConanFile):
   settings = "os", "compiler", "build_type", "arch"
   requires = "catch2/2.13.6", "rapidcheck/20210107", "benchmark/1.5.3"
   generators = "cmake"
   default_options = {"catch2:with_main": True}

   def build(self):
      cmake = CMake(self)
      cmake.configure()
      cmake.build()