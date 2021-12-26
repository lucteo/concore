#!/usr/bin/env python3

import os
import cpp_transform_tool as tr


def main():
    # Description of the input and output files
    cur_dir = os.path.dirname(os.path.realpath(__file__))
    in_dir = f"{cur_dir}/../external/p2300/include"
    out_dir = f"{cur_dir}/../src/include/concore/_gen/c++20"
    files = [
        "__utility.hpp",
        "concepts.hpp",
        "coroutine.hpp",
        "execution.hpp",
        "functional.hpp",
        "stop_token.hpp",
    ]
    comp_args = ["-std=c++20"]

    # Load the transformation rules
    rules = tr.load_rules(f"{cur_dir}/.transform-rules")
    # Apply the transformation for all our files
    for f in files:
        tr.apply_transform(
            f"{in_dir}/{f}", f"{out_dir}/{f}", rules, comp_args, verbose=True
        )


if __name__ == "__main__":
    main()
