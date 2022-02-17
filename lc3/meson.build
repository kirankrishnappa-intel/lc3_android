
inc = include_directories('include')

pkg_mod = import('pkgconfig')

subdir('src')

sndfiledep = dependency('sndfile')

pkg_mod.generate(libraries : lc3lib,
                 version : '1.0',
                 name : 'liblc3',
                 filebase : 'lc3',
                 description : 'LC3 encoder library')

executable('lc3enc', 'lc3encoder.c', link_with : lc3lib,
		include_directories: inc,
		dependencies: sndfiledep)
