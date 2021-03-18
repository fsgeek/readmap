deps = [rt_dep, uuid_dep, pthread_dep, munit_dep]

common_sources = [
    'test_base.c',
]

test_readmap_sources = [
    'testreadmap.c'
]

executable('testreadmap',
            [common_sources, test_readmap_sources],
            dependencies: deps,
            include_directories: [include_dirs, '.'],
            link_with: [readmap_api])
