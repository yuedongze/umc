from distutils.core import setup, Extension

extension_mod = Extension("sony", include_dirs = ['/usr/include', '/usr/local/include'] ,libraries = ['usb'], library_dirs = ['/usr/lib', '/usr/local/lib'], sources = ["ptp.c" , "main.c"])

setup(name = "sony", ext_modules=[extension_mod])