readmap_api_sources = [
    'fdmgr.c',
    'init.c',
    'openclose.c',
    'read.c',
    'write.c',
]

api_dep = [thread_dep, uuid_dep, dl_dep, rt_dep, pthread_dep]

readmap_api = static_library('readmap_api', readmap_api_sources,
                             include_directories: [include_dirs, '.'],
                             dependencies: api_dep,
                             install: false
                             )
