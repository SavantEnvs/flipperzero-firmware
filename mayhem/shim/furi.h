#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <core/common_defines.h>
#include <core/check.h>
#include <core/string.h>

typedef struct FuriPubSub FuriPubSub;

#define FURI_LOG_E(tag, ...) ((void)(tag))
#define FURI_LOG_W(tag, ...) ((void)(tag))
#define FURI_LOG_I(tag, ...) ((void)(tag))
#define FURI_LOG_D(tag, ...) ((void)(tag))
#define FURI_LOG_T(tag, ...) ((void)(tag))

void* furi_record_open(const char* name);
void furi_record_close(const char* name);
