/*
 * Copyright 2022, UNSW
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#define UDP_ECHO_PORT 1235
#define UTILIZATION_PORT 1236

int setup_udp_socket(void);
int setup_utilization_socket(void);