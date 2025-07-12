import os

ucrt64_path = "C:/msys64/ucrt64/bin"

env = Environment(
    CXXCOMSTR='\nCompiling $SOURCE',
    LINKCOMSTR='\nLinking $TARGET\n',
    ENV={
        'PATH': ucrt64_path + os.pathsep + os.environ['PATH'],
        'TEMP': os.environ.get('TEMP', 'C:/Temp'),
        'TMP': os.environ.get('TMP',   'C:/Temp'),
    },
    tools=['mingw'],  # Use 'mingw' to pick up GCC on Windows
    
)

env.Append(CXXFLAGS=['-std=c++20'])

AddOption('--no-lint',
          action='store_true',
          dest='no_lint',
          default=False,
          help='Disable clang-tidy linting')

third_objs, third_env = SConscript('third_party/SConscript', exports='env')

SConscript('app/src/SConscript', variant_dir='build', exports={'env': third_env, 'objects': third_objs})