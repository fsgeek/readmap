
readmap_preload_sources = [
    'close.c',
    'open.c',
    'read.c',
    'write.c',
]

preload_dep = [thread_dep, dl_dep, rt_dep, pthread_dep]

readmap_preload = library('readmap_preload',
                          readmap_preload_sources,
                          version: meson.project_version(),
                          soversion: '1',
                          include_directories: [include_dirs, '.'],
                          dependencies: preload_dep,
                          link_with: [readmap_api],
                          install: true,)

