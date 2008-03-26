/*
    chips.c - Part of sensors, a user-space program for hardware monitoring
    Copyright (C) 1998-2003  Frodo Looijaard <frodol@dds.nl> and
                             Mark D. Studebaker <mdsxyz123@yahoo.com>
    Copyright (C) 2007       Jean Delvare <khali@linux-fr.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
    MA 02110-1301 USA.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "chips.h"
#include "lib/sensors.h"
#include "lib/error.h"

void print_chip_raw(const sensors_chip_name *name)
{
	int a, b, err;
	const sensors_feature *feature;
	const sensors_subfeature *sub;
	char *label;
	double val;

	a = 0;
	while ((feature = sensors_get_features(name, &a))) {
		if (!(label = sensors_get_label(name, feature))) {
			fprintf(stderr, "ERROR: Can't get label of feature "
				"%s!\n", feature->name);
			continue;
		}
		printf("%s:\n", label);
		free(label);

		b = 0;
		while ((sub = sensors_get_all_subfeatures(name, feature, &b))) {
			if (sub->flags & SENSORS_MODE_R) {
				if ((err = sensors_get_value(name, sub->number,
							     &val)))
					fprintf(stderr, "ERROR: Can't get "
						"value of subfeature %s: %s\n",
						sub->name,
						sensors_strerror(err));
				else
					printf("  %s: %.2f\n", sub->name, val);
			} else
				printf("(%s)\n", label);
		}
	}
}

static inline double deg_ctof(double cel)
{
	return cel * (9.0F / 5.0F) + 32.0F;
}

static void print_label(const char *label, int space)
{
	int len = strlen(label)+1;
	printf("%s:%*s", label, space - len, "");
}

static double get_value(const sensors_chip_name *name,
			const sensors_subfeature *sub)
{
	double val;
	int err;

	err = sensors_get_value(name, sub->number, &val);
	if (err) {
		fprintf(stderr, "ERROR: Can't get value of subfeature %s: %s\n",
			sub->name, sensors_strerror(err));
		val = 0;
	}
	return val;
}

static int get_label_size(const sensors_chip_name *name)
{
	int i;
	const sensors_feature *iter;
	char *label;
	unsigned int max_size = 11;	/* 11 as minumum label width */

	i = 0;
	while ((iter = sensors_get_features(name, &i))) {
		if ((label = sensors_get_label(name, iter)) &&
		    strlen(label) > max_size)
			max_size = strlen(label);
		free(label);
	}
	return max_size + 1;
}

static void print_temp_limits(double limit1, double limit2,
			      const char *name1, const char *name2, int alarm)
{
	if (fahrenheit) {
		limit1 = deg_ctof(limit1);
		limit2 = deg_ctof(limit2);
        }

	if (name2) {
		printf("(%-4s = %+5.1f%s, %-4s = %+5.1f%s)  ",
		       name1, limit1, degstr,
		       name2, limit2, degstr);
	} else if (name1) {
		printf("(%-4s = %+5.1f%s)                  ",
		       name1, limit1, degstr);
	} else {
		printf("                                  ");
	}

	if (alarm)
		printf("ALARM  ");
}

static void print_chip_temp(const sensors_chip_name *name,
			    const sensors_feature *feature,
			    int label_size)
{
	const sensors_subfeature *sf, *sfmin, *sfmax, *sfcrit, *sfhyst;
	double val, limit1, limit2;
	const char *s1, *s2;
	int alarm, crit_displayed = 0;
	char *label;

	if (!(label = sensors_get_label(name, feature))) {
		fprintf(stderr, "ERROR: Can't get label of feature %s!\n",
			feature->name);
		return;
	}
	print_label(label, label_size);
	free(label);

	sf = sensors_get_subfeature(name, feature,
				    SENSORS_SUBFEATURE_TEMP_ALARM);
	alarm = sf && get_value(name, sf);

	sfmin = sensors_get_subfeature(name, feature,
				       SENSORS_SUBFEATURE_TEMP_MIN);
	sfmax = sensors_get_subfeature(name, feature,
				       SENSORS_SUBFEATURE_TEMP_MAX);
	sfcrit = sensors_get_subfeature(name, feature,
					SENSORS_SUBFEATURE_TEMP_CRIT);
	if (sfmax) {
		sf = sensors_get_subfeature(name, feature,
					SENSORS_SUBFEATURE_TEMP_MAX_ALARM);
		if (sf && get_value(name, sf))
			alarm |= 1;

     		if (sfmin) {
			limit1 = get_value(name, sfmin);
			s1 = "low";
			limit2 = get_value(name, sfmax);
			s2 = "high";

			sf = sensors_get_subfeature(name, feature,
					SENSORS_SUBFEATURE_TEMP_MIN_ALARM);
			if (sf && get_value(name, sf))
				alarm |= 1;
		} else {
			limit1 = get_value(name, sfmax);
			s1 = "high";

			sfhyst = sensors_get_subfeature(name, feature,
					SENSORS_SUBFEATURE_TEMP_MAX_HYST);
			if (sfhyst) {
				limit2 = get_value(name, sfhyst);
				s2 = "hyst";
			} else if (sfcrit) {
				limit2 = get_value(name, sfcrit);
				s2 = "crit";

				sf = sensors_get_subfeature(name, feature,
					SENSORS_SUBFEATURE_TEMP_CRIT_ALARM);
				if (sf && get_value(name, sf))
					alarm |= 1;
				crit_displayed = 1;
			} else {
				limit2 = 0;
				s2 = NULL;
			}
		}
	} else if (sfcrit) {
		limit1 = get_value(name, sfcrit);
		s1 = "crit";

		sfhyst = sensors_get_subfeature(name, feature,
					SENSORS_SUBFEATURE_TEMP_CRIT_HYST);
		if (sfhyst) {
			limit2 = get_value(name, sfhyst);
			s2 = "hyst";
		} else {
			limit2 = 0;
			s2 = NULL;
		}

		sf = sensors_get_subfeature(name, feature,
					SENSORS_SUBFEATURE_TEMP_CRIT_ALARM);
		if (sf && get_value(name, sf))
			alarm |= 1;
		crit_displayed = 1;
	} else {
		limit1 = limit2 = 0;
		s1 = s2 = NULL;
	}


	sf = sensors_get_subfeature(name, feature,
				    SENSORS_SUBFEATURE_TEMP_FAULT);
	if (sf && get_value(name, sf)) {
		printf("   FAULT  ");
	} else {
		sf = sensors_get_subfeature(name, feature,
					    SENSORS_SUBFEATURE_TEMP_INPUT);
		if (sf) {
			val = get_value(name, sf);
			if (fahrenheit)
				val = deg_ctof(val);
			printf("%+6.1f%s  ", val, degstr);
		} else
			printf("     N/A  ");
	}
	print_temp_limits(limit1, limit2, s1, s2, alarm);

	if (!crit_displayed && sfcrit) {
		limit1 = get_value(name, sfcrit);
		s1 = "crit";

		sfhyst = sensors_get_subfeature(name, feature,
					SENSORS_SUBFEATURE_TEMP_CRIT_HYST);
		if (sfhyst) {
			limit2 = get_value(name, sfhyst);
			s2 = "hyst";
		} else {
			limit2 = 0;
			s2 = NULL;
		}

		sf = sensors_get_subfeature(name, feature,
					SENSORS_SUBFEATURE_TEMP_CRIT_ALARM);
		alarm = sf && get_value(name, sf);

		printf("\n%*s", label_size + 10, "");
		print_temp_limits(limit1, limit2, s1, s2, alarm);
	}

	/* print out temperature sensor info */
	sf = sensors_get_subfeature(name, feature,
				    SENSORS_SUBFEATURE_TEMP_TYPE);
	if (sf) {
		int sens = (int)get_value(name, sf);

		/* older kernels / drivers sometimes report a beta value for
		   thermistors */
		if (sens > 1000)
			sens = 4;

		printf("sensor = %s", sens == 0 ? "disabled" :
		       sens == 1 ? "diode" :
		       sens == 2 ? "transistor" :
		       sens == 3 ? "thermal diode" :
		       sens == 4 ? "thermistor" :
		       sens == 5 ? "AMD AMDSI" :
		       sens == 6 ? "Intel PECI" : "unknown");
	}
	printf("\n");
}

static void print_chip_in(const sensors_chip_name *name,
			  const sensors_feature *feature,
			  int label_size)
{
	const sensors_subfeature *sf, *sfmin, *sfmax;
	double alarm_max, alarm_min;
	char *label;

	if (!(label = sensors_get_label(name, feature))) {
		fprintf(stderr, "ERROR: Can't get label of feature %s!\n",
			feature->name);
		return;
	}
	print_label(label, label_size);
	free(label);

	sf = sensors_get_subfeature(name, feature,
				    SENSORS_SUBFEATURE_IN_INPUT);
	if (sf)
		printf("%+6.2f V", get_value(name, sf));
	else
		printf("     N/A");

	sfmin = sensors_get_subfeature(name, feature,
				       SENSORS_SUBFEATURE_IN_MIN);
	sfmax = sensors_get_subfeature(name, feature,
				       SENSORS_SUBFEATURE_IN_MAX);
	if (sfmin && sfmax)
		printf("  (min = %+6.2f V, max = %+6.2f V)",
		       get_value(name, sfmin),
		       get_value(name, sfmax));
	else if (sfmin)
		printf("  (min = %+6.2f V)",
		       get_value(name, sfmin));
	else if (sfmax)
		printf("  (max = %+6.2f V)",
		       get_value(name, sfmax));

	sf = sensors_get_subfeature(name, feature,
				    SENSORS_SUBFEATURE_IN_ALARM);
	sfmin = sensors_get_subfeature(name, feature,
				       SENSORS_SUBFEATURE_IN_MIN_ALARM);
	sfmax = sensors_get_subfeature(name, feature,
				       SENSORS_SUBFEATURE_IN_MAX_ALARM);
	if (sfmin || sfmax) {
		alarm_max = sfmax ? get_value(name, sfmax) : 0;
		alarm_min = sfmin ? get_value(name, sfmin) : 0;

		if (alarm_min || alarm_max) {
			printf(" ALARM (");

			if (alarm_min)
				printf("MIN");
			if (alarm_max)
				printf("%sMAX", (alarm_min) ? ", " : "");

			printf(")");
		}
	} else if (sf) {
		printf("   %s",
		       get_value(name, sf) ? "ALARM" : "");
	}

	printf("\n");
}

static void print_chip_fan(const sensors_chip_name *name,
			   const sensors_feature *feature,
			   int label_size)
{
	const sensors_subfeature *sf, *sfmin, *sfdiv;
	char *label;

	if (!(label = sensors_get_label(name, feature))) {
		fprintf(stderr, "ERROR: Can't get label of feature %s!\n",
			feature->name);
		return;
	}
	print_label(label, label_size);
	free(label);

	sf = sensors_get_subfeature(name, feature,
				    SENSORS_SUBFEATURE_FAN_FAULT);
	if (sf && get_value(name, sf))
		printf("   FAULT");
	else {
		sf = sensors_get_subfeature(name, feature,
					    SENSORS_SUBFEATURE_FAN_INPUT);
		if (sf)
			printf("%4.0f RPM", get_value(name, sf));
		else
			printf("     N/A");
	}

	sfmin = sensors_get_subfeature(name, feature,
				       SENSORS_SUBFEATURE_FAN_MIN);
	sfdiv = sensors_get_subfeature(name, feature,
				       SENSORS_SUBFEATURE_FAN_DIV);
	if (sfmin && sfdiv)
		printf("  (min = %4.0f RPM, div = %1.0f)",
		       get_value(name, sfmin),
		       get_value(name, sfdiv));
	else if (sfmin)
		printf("  (min = %4.0f RPM)",
		       get_value(name, sfmin));
	else if (sfdiv)
		printf("  (div = %1.0f)",
		       get_value(name, sfdiv));

	sf = sensors_get_subfeature(name, feature,
				    SENSORS_SUBFEATURE_FAN_ALARM);
	if (sf && get_value(name, sf)) {
		printf("  ALARM");
	}

	printf("\n");
}

static void print_chip_vid(const sensors_chip_name *name,
			   const sensors_feature *feature,
			   int label_size)
{
	char *label;
	const sensors_subfeature *subfeature;
	double vid;

	subfeature = sensors_get_subfeature(name, feature,
					    SENSORS_SUBFEATURE_VID);
	if (!subfeature)
		return;

	if ((label = sensors_get_label(name, feature))
	 && !sensors_get_value(name, subfeature->number, &vid)) {
		print_label(label, label_size);
		printf("%+6.3f V\n", vid);
	}
	free(label);
}

static void print_chip_beep_enable(const sensors_chip_name *name,
				   const sensors_feature *feature,
				   int label_size)
{
	char *label;
	const sensors_subfeature *subfeature;
	double beep_enable;

	subfeature = sensors_get_subfeature(name, feature,
					    SENSORS_SUBFEATURE_BEEP_ENABLE);
	if (!subfeature)
		return;

	if ((label = sensors_get_label(name, feature))
	 && !sensors_get_value(name, subfeature->number, &beep_enable)) {
		print_label(label, label_size);
		printf("%s\n", beep_enable ? "enabled" : "disabled");
	}
	free(label);
}

void print_chip(const sensors_chip_name *name)
{
	const sensors_feature *feature;
	int i, label_size;

	label_size = get_label_size(name);

	i = 0;
	while ((feature = sensors_get_features(name, &i))) {
		switch (feature->type) {
		case SENSORS_FEATURE_TEMP:
			print_chip_temp(name, feature, label_size);
			break;
		case SENSORS_FEATURE_IN:
			print_chip_in(name, feature, label_size);
			break;
		case SENSORS_FEATURE_FAN:
			print_chip_fan(name, feature, label_size);
			break;
		case SENSORS_FEATURE_VID:
			print_chip_vid(name, feature, label_size);
			break;
		case SENSORS_FEATURE_BEEP_ENABLE:
			print_chip_beep_enable(name, feature, label_size);
			break;
		default:
			continue;
		}
	}
}
