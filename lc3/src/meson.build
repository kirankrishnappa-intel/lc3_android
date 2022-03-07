lc3_sources = [
	'attdet.c',
	'bits.c',
	'bwdet.c',
	'energy.c',
	'lc3.c',
	'ltpf.c',
	'mdct.c',
	'plc.c',
	'sns.c',
	'spec.c',
	'tables.c',
	'tns.c'
]

cc = meson.get_compiler('c')

lib_m = cc.find_library('m')

lc3lib = shared_library('lc3', lc3_sources,
			dependencies: lib_m,
			include_directories: inc,
			install: true)

