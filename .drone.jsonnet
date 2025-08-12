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
  name: "test-on-windows",
  platform: {
    os: "windows",
    arch: "amd64",
    version: "1904"
  },
  clone: { disable: true },
  trigger: { event: [ "push", "custom" ] },
  steps: Steps
}
]
