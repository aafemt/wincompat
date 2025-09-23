{
  kind: "pipeline",
  name: "Windows 64 build",
  platform: {
    os: "windows",
    arch: "amd64",
    version: "1809"
  },
  clone: { disable: true },
  trigger: { event: [ "push", "custom" ] },
  steps:
  [
    {
      name: "show environment",
      image: "dctl-registry.dcinet:5000/windows-servercore-1809/vs2022",
      environment:
	  {   DCTL_BASE_URL: { from_secret: "gitea_server" },
          DCTL_FB_API_KEY: { from_secret: "fb_api_token" },
          DCTL_IBP_API_KEY: { from_secret: "ibp_api_token" },
          DCTL_WWW_DOCS_UP: { from_secret: "www_docs_up" },
          GITEA_SERVER: "${DRONE_GIT_HTTP_URL%%sib*}",
          GITEA_TOKEN: { from_secret: "gitea_api_key" },
      },
      commands:
      [
        "echo Hello_World",
        "dir env:",
      ],
    },
  ]
}
