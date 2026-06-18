[
/*
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
      name: "Build and test",
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
        "dlltool -d pathcch.def -D KernelBase.dll -l build/libpathcch.a",
        "c:/apps/cbp2make.exe -in testdll.cbp",
        "c:/apps/cbp2make.exe -in tests.cbp",
        "mingw32-make -f testdll.cbp.mak windows",
        "mingw32-make -f tests.cbp.mak windows",
		"cd build",
		"./tests.exe",
      ],
    },
  ]
},
*/
{
  kind: "pipeline",
  name: "Linux explore",
  platform: {
    os: "linux",
    arch: "amd64"
  },
  clone: { disable: false },
  trigger: { event: [ "push", "custom" ] },
  steps:
  [
    {
      name: "Test",
      image: "dctl-registry.dcinet:5000/suse-15.4/gcc-11-suse-15.4",
      environment:
      {  LC_ALL: "en_US.UTF-8"  },
      commands:
      [
        "find /usr -name \"libicuuc.so*\"",
        "echo 'intl_module = builtin {\\\\n icu_versions =' `find /usr -name 'libicuuc.so.??' 2>/dev/null | sed -e 's/.*\\\\.so\\\\.//' | sort -nr` '\\\\n}' >fbintl.conf",
        "cat intl.conf"
/*
        "cd tests",
        "mkdir build",
        "cbp2make -in testdll.cbp",
        "cbp2make -in tests.cbp",
        "make -f testdll.cbp.mak linux",
        "make -f tests.cbp.mak linux",
		"cd build",
		"./tests",
*/
      ],
    },
  ]
},
]
