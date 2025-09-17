local Build = [
        "./configure",
        "make",
        "make test"
];

local Steps = [
    {
      name: "show environment",
//      image: "dctl-registry.dcinet:5000/windows-servercore-1809/vs2002",
      commands:
      [
        "echo $PWD %CD%",
		"MSBuild -version",
		"msbuild -version"
      ]
    },
/*
    {
      name: "package",
      image: "gcc",
      commands: [
        "zip *.* zzz"
      ]
    }
*/
  ];


[
{
  kind: "pipeline",
  name: "Windows 64 build",
  platform: {
    os: "windows",
    arch: "amd64",
    version: "1809"
  },
  clone: { depth: 1 },
  trigger: { event: [ "push", "custom" ] },
  steps:
  [
    {
      name: "show environment",
      image: "dctl-registry.dcinet:5000/windows-servercore-1809/vs2022",
      commands:
      [
        "echo Hello_World",
        "dir env:",
      ]
    },
    {
      name: "release",
      image: "dctl-registry.dcinet:5000/windows-nanoserver-1809/drone-gitea-release-win",
      settings:
      {
        files: [ "README.md" ],    // files to upload to Gitea Release, globs are allowed. array, required  Default: none */
        file_exists: "overwrite",  // what to do if a file asset already exists,
                                // supported values - overwrite, skip and fail.  string optional  Default: overwrite
        note: "${DRONE_COMMIT_MESSAGE}",  // file or string with notes for the release. string, optional. Default: none
//        title: "Snapshot ${DRONE_COMMIT_SHA}",    // file or string for the title shown in the gitea release. string, optional. Default: none
        insecure: true,        // visit base_url via insecure https protocol. boolean, optional. Default: false
                            // Perhaps we can read the secret for PLUGIN_BASE_URL and test for http://

        draft: true,       // For now always true - we will set manually. Default: false
        prerelease: true,  //For now always true - we will set manually.  Default: false
                        // Need to test for FB_BUILD_TYPE=T
        api_key: { from_secret: "gitea_api_key" },     // Gitea API key  string, required  Default: none
        base_url: "https://scm.ibphoenix.com:3000", // Gitea base URL string, required  Default: none
//        checksum: ["md5"],                // checksum takes hash methods to include in your Gitea release for the files specified.
//                                     // Supported hash methods include - md5, sha1, sha256, sha512, adler32, and crc32.
//                                     // string, optional   Default: none
        verbose: true,
      },
    }
  ]
}
]
