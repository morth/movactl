#ifndef SERIALIZE_H
#define SERIALIZE_H

#include <sys/types.h>

#include "api.h"

#define SERIALIZE_OK 0
#define SERIALIZE_UNKNOWN 1
#define SERIALIZE_BUFFER_FULL 2
#define SERIALIZE_INVALID 3
#define SERIALIZE_SYSCALL_ERR -1

enum ma_type
{
	matype_EOD, matype_bool, matype_int, matype_source, matype_source_state, matype_digital_signal_format,
	matype_sampling_frequency, matype_surround_mode, matype_dolby_headphone_mode, matype_tuner_band, matype_tuner_mode,
	matype_string
};

int serialize_bool (void **buf, size_t *buflen, size_t maxlen, enum ma_bool value);
int serialize_int (void **buf, size_t *buflen, size_t maxlen, int value);
int serialize_source (void **buf, size_t *buflen, size_t maxlen, enum ma_source value);
int serialize_source_state (void **buf, size_t *buflen, size_t maxlen, enum ma_source_state value);
int serialize_digital_signal_format (void **buf, size_t *buflen, size_t maxlen, enum ma_digital_signal_format value);
int serialize_sampling_frequency (void **buf, size_t *buflen, size_t maxlen, enum ma_sampling_frequency value);
int serialize_surround_mode (void **buf, size_t *buflen, size_t maxlen, enum ma_surround_mode value);
int serialize_dolby_headphone_mode (void **buf, size_t *buflen, size_t maxlen, enum ma_dolby_headphone_mode value);
int serialize_tuner_band (void **buf, size_t *buflen, size_t maxlen, enum ma_tuner_band value);
int serialize_tuner_mode (void **buf, size_t *buflen, size_t maxlen, enum ma_tuner_mode value);
int serialize_string (void **buf, size_t *buflen, size_t maxlen, const char *value);

int unserialize_type (char **buf, size_t buflen, enum ma_type *value);
int unserialize_bool (char **buf, size_t buflen, enum ma_bool *value);
int unserialize_int (char **buf, size_t buflen, int *value);
int unserialize_source (char **buf, size_t buflen, enum ma_source *value);
int unserialize_source_state (char **buf, size_t buflen, enum ma_source_state *value);
int unserialize_digital_signal_format (char **buf, size_t buflen, enum ma_digital_signal_format *value);
int unserialize_sampling_frequency (char **buf, size_t buflen, enum ma_sampling_frequency *value);
int unserialize_surround_mode (char **buf, size_t buflen, enum ma_surround_mode *value);
int unserialize_dolby_headphone_mode (char **buf, size_t buflen, enum ma_dolby_headphone_mode *value);
int unserialize_tuner_band (char **buf, size_t buflen, enum ma_tuner_band *value);
int unserialize_tuner_mode (char **buf, size_t buflen, enum ma_tuner_mode *value);
int unserialize_string (char **buf, size_t buflen, const char **value);

#endif /*SERIALIZE_H*/
