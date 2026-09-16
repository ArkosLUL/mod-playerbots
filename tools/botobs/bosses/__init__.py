"""One reader per encounter, run directly as `bosses/<boss>.py <trace> [--section]`.

A reader keeps only what its fight needs and takes the rest from raidobs. Its main() is a SECTIONS
table handed to raidobs.cli.run_sections.
"""
