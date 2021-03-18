project('libreadmap', ['c'], version: '0.0.1',
        meson_version: '>= 0.55.3',
        default_options: ['buildtype=debugoptimized'])

platform = host_machine.system()

if platform != 'linux'
    error('This has only been tested on Linux')
endif

cfg = configuration_data()
c_compiler = meson.get_compiler('c')

args_default= ['-D_GNU_SOURCE', '-Dc_std=gnu11']

include_default = '''
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
'''


cfg.set('HAVE_STATX', c_compiler.has_member('struct notexists', 'stx_atime',
    prefix: include_default, args: args_default))

configure_file(output: 'config.h',
               configuration: cfg)

add_project_arguments('-D_REENTRANT', '-DHAVE_CONFIG_H', '-D_GNU_SOURCE', '-D_XOPEN_SOURCE', '-D__USE_XOPEN_EXTENDED',
                      '-Wall', '-Wextra', '-Wno-sign-compare',
                      '-Wstrict-prototypes', '-Wmissing-declarations', '-Wwrite-strings',
                      '-fno-strict-aliasing', language: 'c')

include_dirs = include_directories('include', '.')
thread_dep = dependency('threads')
uuid_dep = c_compiler.find_library('uuid')
dl_dep = c_compiler.find_library('dl', required: false)
rt_dep = c_compiler.find_library('rt')
pthread_dep = c_compiler.find_library('pthread')

munit_dep = dependency('munit', fallback: ['munit', 'munit_dep'])

subdirs = ['api', 'preload', 'tests']

foreach n : subdirs
    subdir(n)
endforeach
