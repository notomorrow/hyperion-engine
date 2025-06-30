#!/bin/bash

fix_flag=""
convert=0
files=()

# ── parse args ───────────────────────────────────────────────────────────────
while (( $# )); do
    case "$1" in
        --fix)     fix_flag="--fix --fix-errors"; shift ;;
        --convert) convert=1;                       shift ;;
        --files)
            shift
            while (( $# )) && [[ $1 != --* ]]; do
                files+=("$1")
                shift
            done
            ;;
        *) shift ;;
    esac
done

# ── collect sources if none provided ─────────────────────────────────────────
if [ ${#files[@]} -eq 0 ]; then
    while IFS= read -r -d '' file; do
        files+=("$file")
    done < <(find src -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' -o -name '*.inl' \) -print0)
fi

# ── optional snake→camel pass ────────────────────────────────────────────────
if (( convert )); then
    for f in "${files[@]}"; do
        perl -0777 -i -pe '
            use strict;
            use warnings;

            our %kw = map { $_ => 1 } qw(
                alignas alignof asm auto bool break case catch char char8_t char16_t char32_t
                class concept const consteval constexpr constinit const_cast continue co_await
                co_return co_yield decltype default delete do double dynamic_cast else enum
                explicit export extern false float for friend goto if inline int long mutable
                namespace new noexcept nullptr operator private protected public register
                reinterpret_cast requires return short signed sizeof static static_assert
                static_cast struct switch template this thread_local throw true try typedef
                typeid typename union unsigned using virtual void volatile wchar_t while
            );

            sub camel {
                my $s = shift;
                $s =~ s/_([a-z])/\U$1/g;
                return $s;
            }

            s{
                # ── regions we ignore completely ────────────────────────────
                (?:
                    "(?:\\.|[^"\\])*"                               |   # ordinary strings
                    R"[^()\s]{0,16}\((?:.|\n)*?\)[^"]*"             |   # raw strings, any delimiter
                    ^\s*#\s*include\b[^\n]*$                        #   # include line
                ) (*SKIP)(*F)
                |
                # ── candidate identifier ───────────────────────────
                \b
                (?<qual>(?:[A-Za-z_][A-Za-z0-9_]*::)*)              # qualifier chain
                (?<id>(?:[mgs]_)?[a-z]+(?:_[a-z]+)+)                # snake_case id
                \b
            }{
                my ($q,$id) = ($+{qual}, $+{id});

                if (   $q =~ /\bstd::/
                    || $kw{$id}
                    || $id =~ /[A-Z]/
                    || $id =~ /_t$/
                    || $id =~ /^curl_/ )
                {                                                   # untouched
                    $q.$id;
                }
                elsif ($id =~ /^([mgs]_)(.+)$/) {                   # keep prefix
                    $q.$1.camel($2);
                }
                else {                                              # plain snake → camel
                    $q.camel($id);
                }
            }gexm;
        ' "$f"
    done
fi

# ── clang-tidy run ───────────────────────────────────────────────────────────
clang-tidy -p build $fix_flag -format-style=file "${files[@]}" \
  --header-filter=.* \
  --exclude-header-filter="^(.*(thirdparty).*)|(include/.*)" \
  -- -I./src -I./include
