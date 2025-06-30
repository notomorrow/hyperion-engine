#!/bin/bash

fix_flag=""
convert=0
files=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --fix)     fix_flag="--fix --fix-errors"; shift ;;
        --convert) convert=1;                       shift ;;
        --files)
            shift
            while [[ $# -gt 0 && $1 != --* ]]; do
                files+=("$1")
                shift
            done
            ;;
        *) shift ;;
    esac
done

# If no explicit list, grab project sources.
if [ ${#files[@]} -eq 0 ]; then
    while IFS= read -r -d $'\0' file; do
        files+=("$file")
    done < <(find src -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) -print0)
fi

# Optional best-effort snake_case → camelCase pass.
if [ $convert -eq 1 ]; then
    for f in "${files[@]}"; do
        perl -0777 -i -pe '
            sub camel { my $s = shift; $s =~ s/_([a-z])/\U$1/g; $s }
            s/\b([a-z]+(?:_[a-z]+)+)\b/camel($1)/ge;
        ' "$f"
    done
fi

clang-tidy -p build $fix_flag -format-style=file "${files[@]}" \
  --header-filter=.* \
  --exclude-header-filter="^(.*(thirdparty).*)|(include\/.*)" \
  -- -I./src -I./include
