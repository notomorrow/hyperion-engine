import glob
import os
from pathlib import Path
import inspect

def create_files(in_path, out_path):
    headers = glob.glob('{0}/*.h'.format(in_path))

    print("Create path if not exists ", out_path)
    os.makedirs(out_path, exist_ok=True)

    for header in headers:
        filename = os.path.basename(header)
        filename_without_extension = os.path.splitext(filename)[0]
        filename_macro_case = filename_without_extension.upper()

        print("Create ", filename)

        new_filepath = '{0}/{1}'.format(out_path, filename)
        
        #if os.path.exists(new_filepath):
        #    print("File {0} exists, skipping.".format(new_filepath))
        #    continue

        include_path = in_path
        if include_path.startswith("src/"):
            include_path = include_path[len("src/"):]

        f = open(new_filepath, 'w')
        f.write(inspect.cleandoc("""\
            #ifndef HYPERION_V2_BACKEND_{2}_H
            #define HYPERION_V2_BACKEND_{2}_H

            #include <util/defines.h>

            #if HYP_VULKAN
            #include <{0}/{1}>
            #else
            #error Unsupported rendering backend
            #endif

            #endif
            """.format(include_path, filename, filename_macro_case)))
        f.close()

create_files('src/rendering/backend/vulkan', 'src/rendering/backend')
create_files('src/rendering/backend/vulkan/rt', 'src/rendering/backend/rt')