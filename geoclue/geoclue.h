/* Geoclue - A DBus api and wrapper for geography information
 * Copyright (C) 2006 Garmin
 * 
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2 as published by the Free Software Foundation; 
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __ORG_FOINSE_PROJECT_GEOCLUE_GEOCLUE_H__
#define __ORG_FOINSE_PROJECT_GEOCLUE_GEOCLUE_H__

#include <dbus/dbus-glib.h>


G_BEGIN_DECLS


typedef enum _geoclue_returncode
{
    GEOCLUE_SUCCESS                  = 0,
    GEOCLUE_NOT_INITIALIZED          = -1, 
    GEOCLUE_DBUS_ERROR               = -2,
    GEOCLUE_SERVICE_NOT_AVAILABLE    = -3, 
   
} GEOCLUE_RETURNCODE;



    typedef void (*GEOCLUE_CALLBACK)(gdouble lat, gdouble lon, void* userdata);

 

    GEOCLUE_RETURNCODE geoclue_version(int* major, int* minor, int* micro);
    GEOCLUE_RETURNCODE geoclue_init();
    GEOCLUE_RETURNCODE geoclue_close();       
    GEOCLUE_RETURNCODE geoclue_service_provider(char** name);       
    GEOCLUE_RETURNCODE geoclue_current_position ( gdouble* OUT_latitude, gdouble* OUT_longitude );
    GEOCLUE_RETURNCODE geoclue_current_position_error ( gdouble* OUT_latitude_error, gdouble* OUT_longitude_error );
    GEOCLUE_RETURNCODE geoclue_current_altitude ( gdouble* OUT_altitude );
    GEOCLUE_RETURNCODE geoclue_current_velocity ( gdouble* OUT_north_velocity, gdouble* OUT_east_velocity );
    GEOCLUE_RETURNCODE geoclue_current_time ( gint* OUT_year, gint* OUT_month, gint* OUT_day, gint* OUT_hours, gint* OUT_minutes, gint* OUT_seconds );
    GEOCLUE_RETURNCODE geoclue_satellites_in_view ( GArray** OUT_prn_numbers );
    GEOCLUE_RETURNCODE geoclue_satellites_data ( const gint IN_prn_number, gdouble* OUT_elevation, gdouble* OUT_azimuth, gdouble* OUT_signal_noise_ratio );
    GEOCLUE_RETURNCODE geoclue_sun_rise ( const gdouble IN_latitude, const gdouble IN_longitude, const gint IN_year, const gint IN_month, const gint IN_day, gint* OUT_hours, gint* OUT_minutes, gint* OUT_seconds );
    GEOCLUE_RETURNCODE geoclue_sun_set ( const gdouble IN_latitude, const gdouble IN_longitude, const gint IN_year, const gint IN_month, const gint IN_day, gint* OUT_hours, gint* OUT_minutes, gint* OUT_seconds );
    GEOCLUE_RETURNCODE geoclue_moon_rise ( const gdouble IN_latitude, const gdouble IN_longitude, const gint IN_year, const gint IN_month, const gint IN_day, gint* OUT_hours, gint* OUT_minutes, gint* OUT_seconds );
    GEOCLUE_RETURNCODE geoclue_moon_set ( const gdouble IN_latitude, const gdouble IN_longitude, const gint IN_year, const gint IN_month, const gint IN_day, gint* OUT_hours, gint* OUT_minutes, gint* OUT_seconds );
    GEOCLUE_RETURNCODE geoclue_geocode ( const char * IN_street, const char * IN_city, const char * IN_state, const char * IN_zip, gdouble* OUT_latitude, gdouble* OUT_longitude, gint* OUT_return_code );
    GEOCLUE_RETURNCODE geoclue_geocode_free_text ( const char * IN_free_text, gdouble* OUT_latitude, gdouble* OUT_longitude, gint* OUT_return_code );



G_END_DECLS


#endif // __ORG_FOINSE_PROJECT_GEOCLUE_GEOCLUE_H__























