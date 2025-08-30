import os

ucrt64_path = "C:/msys64/ucrt64/bin"

env = Environment(
    CXXCOMSTR='Compiling $SOURCE',
    LINKCOMSTR='\nLinking $TARGET\n',
    ENV={
        'PATH': ucrt64_path + os.pathsep + os.environ['PATH'],
        'TEMP': os.environ.get('TEMP', 'C:/Temp'),
        'TMP': os.environ.get('TMP',   'C:/Temp'),
    },
    tools=['mingw'],  # Use 'mingw' to pick up GCC on Windows
    
)

env.Append(CXXFLAGS=[
    '-std=c++20', 
    '-g',
    '-O3',
    '-static', 
    '-static-libgcc', 
    '-static-libstdc++'
])
env.Append(CCFLAGS=['-D_WIN32'])
env.Append(LINKFLAGS=['-static'])

AddOption('--no-lint',
          action='store_true',
          dest='no_lint',
          default=True,
          help='Disable clang-tidy linting')

third_objs, third_env = SConscript('third_party/SConscript', exports='env')

SConscript('source/app/SConscript', variant_dir='build', duplicate=0, exports={'env': third_env, 'objects': third_objs})