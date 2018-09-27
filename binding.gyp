{
  "targets": [
    {
      "target_name": "users",
      "sources": ["users.cc"],
      "include_dirs": [ "<!(node -e \"require('nan')\")" ]
    }
  ]
}
