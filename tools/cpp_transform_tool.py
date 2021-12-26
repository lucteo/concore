#!/usr/bin/env python3

import argparse
import cpp_transform.rules as rules
from cpp_transform.TransformedUnit import TransformedUnit


def apply_transform(in_file, out_file, rules, comp_args, verbose=False):
    """
    Apply the C++ transformation specified by the given rules object.
    The result is written to `out_file`.
    """
    # Load the input source file, preparing it for transformations
    unit = TransformedUnit(in_file, comp_args)

    if verbose:
        print(f"Generating {out_file}...")
        unit.print_diagnostics()

    # Apply the rules
    for r in rules:
        r.run(unit, verbose)

    # Save the file with transformations to the destination
    unit.save(out_file)


def load_rules(rules_file):
    """Load the given rules file. Returns a list of rules objects"""
    return rules.load_rules(rules_file)


def transform(in_file, out_file, rules_file, comp_args, verbose=False):
    """
    Apply the C++ transformation specified in the rules file to the given input filename.
    The result is written to `out_file`.
    """
    apply_transform(in_file, out_file, load_rules(rules_file), comp_args, verbose)


def _print_tokens(in_file, comp_args):
    """Dumps the list of tokens of the input file"""
    unit = TransformedUnit(in_file, comp_args)
    for t in unit.get_tokens():
        print(t.kind, t.spelling)


def _print_ast(in_file, comp_args):
    """Dumps the AST content of the input file"""
    unit = TransformedUnit(in_file, comp_args)
    print("# kind spelling location extent")

    def _loc_to_str(loc):
        return f"{loc.line}:{loc.column}"

    def _extent_to_str(loc_extent):
        return f"{_loc_to_str(loc_extent.start)}-{_loc_to_str(loc_extent.end)}"

    def _print(node, indent=0):
        # Print the current node
        print("  " * indent, end="")
        print(
            node.kind,
            node.spelling,
            _loc_to_str(node.location),
            _extent_to_str(node.extent),
        )
        # Recurse down to children
        for c in node.get_children():
            _print(c, indent + 1)

    _print(unit.cursor)


def main():
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
        "-c",
        "--comp-arg",
        type=str,
        nargs="*",
        default=["-std=c++20"],
        help="The arguments to be passed to libclang when parsing the file",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        default=False,
        help="Display more information when running the script",
    )
    parser.add_argument(
        "--print-tokens",
        action="store_true",
        default=False,
        help="Dumps the tokens from the input file",
    )
    parser.add_argument(
        "--print-ast",
        action="store_true",
        default=False,
        help="Dumps the AST from the input file",
    )
    args = parser.parse_args()

    if args.print_tokens:
        _print_tokens(args.input, args.comp_arg)
        return

    if args.print_ast:
        _print_ast(args.input, args.comp_arg)
        return

    # Do the transform
    transform(args.input, args.output, args.rules, args.comp_arg, args.verbose)


if __name__ == "__main__":
    main()
