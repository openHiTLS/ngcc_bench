#ifndef JSON_REPORT_H
#define JSON_REPORT_H

#include "cli_types.h"

int write_json_report(const cli_options_t *opts,
                      const run_report_t *report,
                      int overall_failed);

#endif
