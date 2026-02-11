from conan import ConanFile

class MypkgProject(ConanFile):
    name = "mypkg"
    version = "0.1"

    support_sdk_version = "^2.4"
    package_type = "application"

    requires = (
        "battery_calculator/1.0.3@smilingboy",
    )

    generators = (
        "SConsDeps",
        "KconfigDeps",
    )
