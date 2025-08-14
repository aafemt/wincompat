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
        "Set-PSDebug -Trace 1",
        "$PSVersionTable",
        "$PWD",
        "& $env:VS170COMNTOOLS/Launch-VsDevShell.ps1",
		"dir env:",
        "msbuild -version",
        "cmake --version",
      ]
    },
  ]
}
]
