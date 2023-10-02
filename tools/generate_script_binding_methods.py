from mako.template import Template

import os

script_directory = os.path.dirname(__file__)
output_filepath = os.path.normpath(os.path.join(script_directory, '..', 'src', 'script', 'ScriptBindingDef.generated.hpp'))

get_binding_path = lambda f: os.path.normpath(os.path.join(script_directory, "bindings", "{}.inc".format(f)))

begin, end = [Template(filename=get_binding_path(f)).render() for f in ['begin', 'end']]

files = ['member_functions', 'functions', 'constructors']

combined = [begin]

for f in files:
    bindings_text = Template(filename=get_binding_path(f))

    combined.append("#pragma region {}\n\n".format(f.title()))
    combined.append(bindings_text.render())
    combined.append("\n\n#pragma endregion")

combined.append(end)

f = open(output_filepath, "w")
f.write("\n".join(combined))
f.close()

print("Wrote to file: {}".format(output_filepath))