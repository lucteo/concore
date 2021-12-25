#!/usr/bin/env python3

import argparse
import yaml
import re


def apply_transform(in_file, out_file, rules, verbose=False):
    """
    Apply the C++ transformation specified by the given rules object.
    The result is written to `out_file`.
    """
    # Read the input file
    with open(in_file) as f:
        content = f.read()

    if verbose:
        print(f"Generating {out_file}...")

    # Apply the rules
    for r in rules:
        content = _apply_rule(r, content, verbose)

    # Write the final content to the file
    with open(out_file, "w") as f:
        f.write(content)


def _apply_rule(rule, content, verbose):
    """Applies the given rule to the given C++ file content; returns the new content"""
    for rule_name, params in rule.items():
        if rule_name == "NamespaceReplace":
            content = _apply_namespace_replace(
                content, params["from"], params["to"], verbose
            )
        elif rule_name == "HeaderReplace":
            content = _apply_header_replace(
                content, params["from"], params["to"], verbose
            )
        else:
            print(f"ERROR: Unknown rule '{rule_name}'")
    return content


def _apply_namespace_replace(content, from_str, to_str, verbose):
    """Apply a namespace replace rule"""
    if verbose:
        print(f"  trying namespace replace {from_str} -> {to_str}")
    from_re = r"\s*\:\:\s*".join(from_str.split("::"))
    content, count = re.subn(
        fr"(namespace\s+){from_re}(\s*\{{)", fr"\g<1>{to_str}\g<2>", content
    )
    if verbose and count > 0:
        print(f"    {count} replacements made")
    return content


def _apply_header_replace(content, from_str, to_str, verbose):
    """Apply a header replace rule"""
    if verbose:
        print(f"  trying header replace {from_str} -> {to_str}")
    # escape non-alphanumeric characters
    from_re = re.sub(r"(\W)", r"\\\g<1>", from_str)
    content, count = re.subn(
        fr"(\s*\#\s*include\s+){from_re}(\s*)", fr"\g<1>{to_str}\g<2>", content
    )
    if verbose and count > 0:
        print(f"    {count} replacements made")
    return content


def load_rules(rules_file):
    """Load the given rules file. Returns a rules object"""
    with open(rules_file) as f:
        return yaml.safe_load(f)


def transform(in_file, out_file, rules_file, verbose=False):
    """
    Apply the C++ transformation specified in the rules file to the given input filename.
    The result is written to `out_file`.
    """
    rules = load_rules(rules_file)
    apply_transform(in_file, out_file, rules, verbose)


def main():
    print("C++ transform tool, copyright (c) 2021 Lucian Radu Teodorescu\n")

    parser = argparse.ArgumentParser(
        description="Apply code transformations to C++ source files"
    )
    parser.add_argument("input", type=str, help="Input file(s) to be transformed")
    parser.add_argument("output", type=str, help="The name of the file to be generated")
    parser.add_argument(
        "--rules",
        type=str,
        default=".transform-rules",
        help="File containing the transformation rules to be applied",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        default=False,
        help="Display more information when running the script",
    )
    args = parser.parse_args()

    # Do the transform
    transform(args.input, args.output, args.rules, args.verbose)


if __name__ == "__main__":
    main()
