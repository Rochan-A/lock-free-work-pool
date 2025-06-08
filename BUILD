load("@hedron_compile_commands//:refresh_compile_commands.bzl", "refresh_compile_commands")

refresh_compile_commands(
    name = "refresh_compile_commands",
    exclude_external_sources = True,
    exclude_headers = "external",

    # Specify the targets of interest.
    # For example, specify a dict of targets and any flags required to build.
    targets = {
        "//work_pool/...": "",
        "//signal_tree/...": "",
    },
)