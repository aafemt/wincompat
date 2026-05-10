{
  kind: "pipeline",
  name: "Windows 64 build",
  platform: {
    os: "windows",
    arch: "amd64",
    version: "1809"
  },
  clone: { disable: false },
  trigger: { event: [ "push", "custom" ] },
  steps:
  [
    {
      name: "Windows",
      image: "dctl-registry.dcinet:5000/windows-servercore-1809/mingw-gcc81c-vs2010",
      environment:
	  {   DCTL_BASE_URL: { from_secret: "gitea_server" },
          DCTL_FB_API_KEY: { from_secret: "fb_api_token" },
          DCTL_IBP_API_KEY: { from_secret: "ibp_api_token" },
          DCTL_WWW_DOCS_UP: { from_secret: "www_docs_up" },
          GITEA_TOKEN: { from_secret: "gitea_api_key" },
      },
      commands:
      [
        "cd tests",
        "mkdir build",
        "dlltool -d pathcch.def -D api-ms-win-core-path-l1-1-0.dll -l build/libpathcch.a",
        "c:/apps/cbp2make.exe -in testdll.cbp",
        "c:/apps/cbp2make.exe -in tests.cbp",
        "mingw32-make -f testdll.cbp.mak windows",
        "mingw32-make -f tests.cbp.mak windows",
		"cd build",
		"& ./tests",
      ],
    },
  ]
}
