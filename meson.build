project('D2VSource', 'c', 'cpp',
    default_options: ['buildtype=release', 'warning_level=2', 'b_lto=true', 'b_ndebug=if-release', 'cpp_std=c++17'],
    license: 'LGPL-2.1-or-later',
    license_files: 'COPYING',
    meson_version: '>=1.2.3',
    version: '1.4',
)

sources = [
    'src/core/compat.cpp',
    'src/core/compat.hpp',
    'src/core/d2v.cpp',
    'src/core/d2v.hpp',
    'src/core/decode.cpp',
    'src/core/decode.hpp',
    'src/core/gop.hpp',
    'src/vs4/applyrff4.cpp',
    'src/vs4/applyrff4.hpp',
    'src/vs4/d2vsource4.cpp',
    'src/vs4/d2vsource4.hpp',
    'src/vs4/directrender4.cpp',
    'src/vs4/directrender4.hpp',
    'src/vs4/vapoursynth4.cpp'
]

libavcodec_dep = dependency('libavcodec')
libavutil_dep = dependency('libavutil')
libavformat_dep = dependency('libavformat')

deps = [libavcodec_dep, libavutil_dep, libavformat_dep]

incdir = include_directories(
    run_command(
        find_program('python', 'python3'),
        '-c',
        'import vapoursynth as vs; print(vs.get_include())',
        check: true,
    ).stdout().strip(),
)

py = import('python').find_installation(pure: false)

shared_module('d2vsource',
    sources,
    dependencies : deps,
    gnu_symbol_visibility: 'hidden',
    include_directories: incdir,
    install: true,
    install_dir: py.get_install_dir() / 'vapoursynth/plugins',
    name_prefix: '',
)