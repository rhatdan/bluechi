# SPDX-License-Identifier: GPL-2.0-or-later

# node build configuration

node_src = [
  'main.c',
  'agent.c',
  'proxy.c'
]

executable(
  'hirte-agent',
  node_src,
  dependencies: [
    systemd_dep,
  ],
  link_with: [
    hirte_lib,
  ],
  c_args: common_cflags,
  include_directories: include_directories('..'),
  install: true
)
