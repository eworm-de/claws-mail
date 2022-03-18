/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2022 the Claws Mail team and Colin Leroy
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#ifdef USE_GNUTLS

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <gnutls/crypto.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "prefs_common.h"
#include "defs.h"
#include "ssl_certificate.h"
#include "utils.h"
#include "alertpanel.h"
#include "hooks.h"

static gboolean sslcertwindow_ask_new_cert(SSLCertificate *cert);
static gboolean sslcertwindow_ask_expired_cert(SSLCertificate *cert);
static gboolean sslcertwindow_ask_changed_cert(SSLCertificate *old_cert, SSLCertificate *new_cert);

static GtkWidget *cert_presenter(SSLCertificate *cert)
{
	GtkWidget *vbox = NULL;
	GtkWidget *hbox = NULL;
	GtkWidget *frame_owner = NULL;
	GtkWidget *frame_signer = NULL;
	GtkWidget *frame_status = NULL;
	GtkWidget *owner_table = NULL;
	GtkWidget *signer_table = NULL;
	GtkWidget *status_table = NULL;
	GtkWidget *label = NULL;
	
	char *issuer_commonname, *issuer_location, *issuer_organization;
	char *subject_commonname, *subject_location, *subject_organization;
	char *sig_status, *exp_date;
	char *sha1_fingerprint, *sha256_fingerprint, *fingerprint;
	size_t n;
	char buf[100];
	unsigned char md[128];	
	char *tmp;
	time_t exp_time_t;
	struct tm lt;
	guint ret;

	/* issuer */	
	issuer_commonname = g_malloc(BUFFSIZE);
	issuer_location = g_malloc(BUFFSIZE);
	issuer_organization = g_malloc(BUFFSIZE);
	subject_commonname = g_malloc(BUFFSIZE);
	subject_location = g_malloc(BUFFSIZE);
	subject_organization = g_malloc(BUFFSIZE);

	n = BUFFSIZE;
	if (gnutls_x509_crt_get_issuer_dn_by_oid(cert->x509_cert, 
		GNUTLS_OID_X520_COMMON_NAME, 0, 0, issuer_commonname, &n))
		strncpy(issuer_commonname, _("<not in certificate>"), BUFFSIZE-1);
	n = BUFFSIZE;

	if (gnutls_x509_crt_get_issuer_dn_by_oid(cert->x509_cert, 
		GNUTLS_OID_X520_LOCALITY_NAME, 0, 0, issuer_location, &n)) {
		if (gnutls_x509_crt_get_issuer_dn_by_oid(cert->x509_cert, 
			GNUTLS_OID_X520_COUNTRY_NAME, 0, 0, issuer_location, &n)) {
			strncpy(issuer_location, _("<not in certificate>"), BUFFSIZE-1);
		}
	} else {
		tmp = g_malloc(BUFFSIZE);
		if (gnutls_x509_crt_get_issuer_dn_by_oid(cert->x509_cert, 
			GNUTLS_OID_X520_COUNTRY_NAME, 0, 0, tmp, &n) == 0) {
			strncat(issuer_location, ", ", BUFFSIZE-strlen(issuer_location)-1);
			strncat(issuer_location, tmp, BUFFSIZE-strlen(issuer_location)-1);
		}
		g_free(tmp);
	}

	n = BUFFSIZE;
	if (gnutls_x509_crt_get_issuer_dn_by_oid(cert->x509_cert, 
		GNUTLS_OID_X520_ORGANIZATION_NAME, 0, 0, issuer_organization, &n))
		strncpy(issuer_organization, _("<not in certificate>"), BUFFSIZE-1);

	n = BUFFSIZE;
	if (gnutls_x509_crt_get_dn_by_oid(cert->x509_cert, 
		GNUTLS_OID_X520_COMMON_NAME, 0, 0, subject_commonname, &n))
		strncpy(subject_commonname, _("<not in certificate>"), BUFFSIZE-1);
	n = BUFFSIZE;

	if (gnutls_x509_crt_get_dn_by_oid(cert->x509_cert, 
		GNUTLS_OID_X520_LOCALITY_NAME, 0, 0, subject_location, &n)) {
		if (gnutls_x509_crt_get_dn_by_oid(cert->x509_cert, 
			GNUTLS_OID_X520_COUNTRY_NAME, 0, 0, subject_location, &n)) {
			strncpy(subject_location, _("<not in certificate>"), BUFFSIZE-1);
		}
	} else {
		tmp = g_malloc(BUFFSIZE);
		if (gnutls_x509_crt_get_dn_by_oid(cert->x509_cert, 
			GNUTLS_OID_X520_COUNTRY_NAME, 0, 0, tmp, &n) == 0) {
			strncat(subject_location, ", ", BUFFSIZE-strlen(subject_location)-1);
			strncat(subject_location, tmp, BUFFSIZE-strlen(subject_location)-1);
		}
		g_free(tmp);
	}

	n = BUFFSIZE;
	if (gnutls_x509_crt_get_dn_by_oid(cert->x509_cert, 
		GNUTLS_OID_X520_ORGANIZATION_NAME, 0, 0, subject_organization, &n))
		strncpy(subject_organization, _("<not in certificate>"), BUFFSIZE-1);
		
	exp_time_t = gnutls_x509_crt_get_expiration_time(cert->x509_cert);

	memset(buf, 0, sizeof(buf));
	if (exp_time_t > 0) {
		fast_strftime(buf, sizeof(buf)-1, prefs_common.date_format, localtime_r(&exp_time_t, &lt));
		exp_date = (*buf) ? g_strdup(buf):g_strdup("?");
	} else
		exp_date = g_strdup("");

	/* fingerprints */
	n = 0;
	memset(md, 0, sizeof(md));
	if ((ret = gnutls_x509_crt_get_fingerprint(cert->x509_cert, GNUTLS_DIG_SHA1, md, &n)) == GNUTLS_E_SHORT_MEMORY_BUFFER) {
		if (n <= sizeof(md))
			ret = gnutls_x509_crt_get_fingerprint(cert->x509_cert, GNUTLS_DIG_SHA1, md, &n);
	}

	if (ret != 0) {
		g_warning("failed to obtain SHA1 fingerprint: %d", ret);
		sha1_fingerprint = g_strdup("-");
	} else {
		sha1_fingerprint = readable_fingerprint(md, (int)n);
	}

	n = 0;
	memset(md, 0, sizeof(md));
	if ((ret = gnutls_x509_crt_get_fingerprint(cert->x509_cert, GNUTLS_DIG_SHA256, md, &n)) == GNUTLS_E_SHORT_MEMORY_BUFFER) {
		if (n <= sizeof(md))
			ret = gnutls_x509_crt_get_fingerprint(cert->x509_cert, GNUTLS_DIG_SHA256, md, &n);
	}

	if (ret != 0) {
		g_warning("failed to obtain SHA256 fingerprint: %d", ret);
		sha256_fingerprint = g_strdup("-");
	} else {
		sha256_fingerprint = readable_fingerprint(md, (int)n);
	}


	/* signature */
	sig_status = ssl_certificate_check_signer(cert, cert->status);

	if (sig_status==NULL)
		sig_status = g_strdup_printf(_("Correct%s"),exp_time_t < time(NULL)? _(" (expired)"): "");
	else if (exp_time_t < time(NULL))
			  sig_status = g_strconcat(sig_status,_(" (expired)"),NULL);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	
	frame_owner  = gtk_frame_new(_("Owner"));
	frame_signer = gtk_frame_new(_("Signer"));
	frame_status = gtk_frame_new(_("Status"));
	
	owner_table = gtk_grid_new();
	signer_table = gtk_grid_new();
	status_table = gtk_grid_new();
	
	label = gtk_label_new(_("Name: "));
	gtk_label_set_xalign(GTK_LABEL(label), 1.0);
	gtk_grid_attach(GTK_GRID(owner_table), label, 0, 0, 1, 1);
	gtk_widget_set_hexpand(label, TRUE);
	gtk_widget_set_halign(label, GTK_ALIGN_FILL);

	label = gtk_label_new(subject_commonname);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(owner_table), label, 1, 0, 1, 1);
	gtk_widget_set_hexpand(label, TRUE);
	gtk_widget_set_halign(label, GTK_ALIGN_FILL);
	
	label = gtk_label_new(_("Organization: "));
	gtk_label_set_xalign(GTK_LABEL(label), 1.0);
	gtk_grid_attach(GTK_GRID(owner_table), label, 0, 1, 1, 1);
	gtk_widget_set_hexpand(label, TRUE);
	gtk_widget_set_halign(label, GTK_ALIGN_FILL);

	label = gtk_label_new(subject_organization);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(owner_table), label, 1, 1, 1, 1);
	gtk_widget_set_hexpand(label, TRUE);
	gtk_widget_set_halign(label, GTK_ALIGN_FILL);
	
	label = gtk_label_new(_("Location: "));
	gtk_label_set_xalign(GTK_LABEL(label), 1.0);
	gtk_grid_attach(GTK_GRID(owner_table), label, 0, 2, 1, 1);
	gtk_widget_set_hexpand(label, TRUE);
	gtk_widget_set_halign(label, GTK_ALIGN_FILL);

	label = gtk_label_new(subject_location);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(owner_table), label, 1, 2, 1, 1);
	gtk_widget_set_hexpand(label, TRUE);
	gtk_widget_set_halign(label, GTK_ALIGN_FILL);

	label = gtk_label_new(_("Name: "));
	gtk_label_set_xalign(GTK_LABEL(label), 1.0);
	gtk_grid_attach(GTK_GRID(signer_table), label, 0, 0, 1, 1);
	gtk_widget_set_hexpand(label, TRUE);
	gtk_widget_set_halign(label, GTK_ALIGN_FILL);

	label = gtk_label_new(issuer_commonname);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(signer_table), label, 1, 0, 1, 1);
	gtk_widget_set_hexpand(label, TRUE);
	gtk_widget_set_halign(label, GTK_ALIGN_FILL);
	
	label = gtk_label_new(_("Organization: "));
	gtk_label_set_xalign(GTK_LABEL(label), 1.0);
	gtk_grid_attach(GTK_GRID(signer_table), label, 0, 1, 1, 1);
	gtk_widget_set_hexpand(label, TRUE);
	gtk_widget_set_halign(label, GTK_ALIGN_FILL);

	label = gtk_label_new(issuer_organization);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(signer_table), label, 1, 1, 1, 1);
	gtk_widget_set_hexpand(label, TRUE);
	gtk_widget_set_halign(label, GTK_ALIGN_FILL);
	
	label = gtk_label_new(_("Location: "));
	gtk_label_set_xalign(GTK_LABEL(label), 1.0);
	gtk_grid_attach(GTK_GRID(signer_table), label, 0, 2, 1, 1);
	gtk_widget_set_hexpand(label, TRUE);
	gtk_widget_set_halign(label, GTK_ALIGN_FILL);

	label = gtk_label_new(issuer_location);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(signer_table), label, 1, 2, 1, 1);
	gtk_widget_set_hexpand(label, TRUE);
	gtk_widget_set_halign(label, GTK_ALIGN_FILL);

	label = gtk_label_new(_("Fingerprint: \n"));
	gtk_label_set_xalign(GTK_LABEL(label), 1.0);
	gtk_grid_attach(GTK_GRID(status_table), label, 0, 0, 1, 1);
	gtk_widget_set_hexpand(label, TRUE);
	gtk_widget_set_halign(label, GTK_ALIGN_FILL);

	fingerprint = g_strdup_printf("SHA1: %s\nSHA256: %s",
				      sha1_fingerprint, sha256_fingerprint);
	label = gtk_label_new(fingerprint);
	g_free(fingerprint);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(status_table), label, 1, 0, 1, 1);
	gtk_widget_set_hexpand(label, TRUE);
	gtk_widget_set_halign(label, GTK_ALIGN_FILL);

	label = gtk_label_new(_("Signature status: "));
	gtk_label_set_xalign(GTK_LABEL(label), 1.0);
	gtk_grid_attach(GTK_GRID(status_table), label, 0, 1, 1, 1);
	gtk_widget_set_hexpand(label, TRUE);
	gtk_widget_set_halign(label, GTK_ALIGN_FILL);

	label = gtk_label_new(sig_status);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(status_table), label, 1, 1, 1, 1);
	gtk_widget_set_hexpand(label, TRUE);
	gtk_widget_set_halign(label, GTK_ALIGN_FILL);

	label = gtk_label_new(exp_time_t < time(NULL)? _("Expired on: "): _("Expires on: "));
	gtk_label_set_xalign(GTK_LABEL(label), 1.0);
	gtk_grid_attach(GTK_GRID(status_table), label, 0, 2, 1, 1);
	gtk_widget_set_hexpand(label, TRUE);
	gtk_widget_set_halign(label, GTK_ALIGN_FILL);

	label = gtk_label_new(exp_date);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(status_table), label, 1, 2, 1, 1);
	gtk_widget_set_hexpand(label, TRUE);
	gtk_widget_set_halign(label, GTK_ALIGN_FILL);
	
	gtk_container_add(GTK_CONTAINER(frame_owner), GTK_WIDGET(owner_table));
	gtk_container_add(GTK_CONTAINER(frame_signer), GTK_WIDGET(signer_table));
	gtk_container_add(GTK_CONTAINER(frame_status), GTK_WIDGET(status_table));
	
	gtk_box_pack_end(GTK_BOX(hbox), frame_signer, TRUE, TRUE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), frame_owner, TRUE, TRUE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), frame_status, TRUE, TRUE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
	
	gtk_widget_show_all(vbox);
	
	g_free(issuer_commonname);
	g_free(issuer_location);
	g_free(issuer_organization);
	g_free(subject_commonname);
	g_free(subject_location);
	g_free(subject_organization);
	g_free(sha1_fingerprint);
	g_free(sha256_fingerprint);
	g_free(sig_status);
	g_free(exp_date);
	return vbox;
}

static gboolean sslcert_ask_hook(gpointer source, gpointer data)
{
	SSLCertHookData *hookdata = (SSLCertHookData *)source;

	if (hookdata == NULL) {
		return FALSE;
	}
	
	if (prefs_common.skip_ssl_cert_check) {
		hookdata->accept = TRUE;
		return TRUE;
	}

	if (hookdata->old_cert == NULL) {
		if (hookdata->expired)
			hookdata->accept = sslcertwindow_ask_expired_cert(hookdata->cert);
		else
			hookdata->accept = sslcertwindow_ask_new_cert(hookdata->cert);
	} else {
		hookdata->accept = sslcertwindow_ask_changed_cert(hookdata->old_cert, hookdata->cert);
	}

	return TRUE;
}

void sslcertwindow_register_hook(void)
{
	hooks_register_hook(SSLCERT_ASK_HOOKLIST, sslcert_ask_hook, NULL);
}

void sslcertwindow_show_cert(SSLCertificate *cert)
{
	GtkWidget *cert_widget = cert_presenter(cert);
	gchar *buf;
	
	buf = g_strdup_printf(_("TLS certificate for %s"), cert->host);
	alertpanel_full(buf, NULL, "window-close", _("_Close"), NULL, NULL, NULL, NULL,
	 		ALERTFOCUS_FIRST, FALSE, cert_widget, ALERT_NOTICE);
	g_free(buf);
}

static gchar *sslcertwindow_get_invalid_str(SSLCertificate *cert)
{
	gchar *subject_cn = NULL;
	gchar *str = NULL;

	if (ssl_certificate_check_subject_cn(cert))
		return g_strdup("");
	
	subject_cn = ssl_certificate_get_subject_cn(cert);
	
	str = g_strdup_printf(_("Certificate is for %s, but connection is to %s.\n"
				"You may be connecting to a rogue server.\n\n"), 
				subject_cn, cert->host);
	g_free(subject_cn);
	
	return str;
}

static gboolean sslcertwindow_ask_new_cert(SSLCertificate *cert)
{
	gchar *buf, *sig_status;
	AlertValue val;
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *button;
	GtkWidget *cert_widget;
	gchar *invalid_str = sslcertwindow_get_invalid_str(cert);
	const gchar *title;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	buf = g_strdup_printf(_("Certificate for %s is unknown.\n%sDo you want to accept it?"), cert->host, invalid_str);
	g_free(invalid_str);

	label = gtk_label_new(buf);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
	g_free(buf);
	
	sig_status = ssl_certificate_check_signer(cert, cert->status);
	if (sig_status==NULL)
		sig_status = g_strdup(_("Correct"));

	buf = g_strdup_printf(_("Signature status: %s"), sig_status);
	label = gtk_label_new(buf);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
	g_free(buf);
	g_free(sig_status);
	
	button = gtk_expander_new_with_mnemonic(_("_View certificate"));
	gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
	cert_widget = cert_presenter(cert);
	gtk_container_add(GTK_CONTAINER(button), cert_widget);

	if (!ssl_certificate_check_subject_cn(cert))
		title = _("TLS certificate is invalid");
	else
		title = _("TLS certificate is unknown");

	val = alertpanel_full(title, NULL,
			      NULL, _("_Cancel connection"), NULL, _("_Accept and save"),
			      NULL, NULL, ALERTFOCUS_FIRST, FALSE, vbox, ALERT_QUESTION);
	
	return (val == G_ALERTALTERNATE);
}

static gboolean sslcertwindow_ask_expired_cert(SSLCertificate *cert)
{
	gchar *buf, *sig_status;
	AlertValue val;
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *button;
	GtkWidget *cert_widget;
	gchar *invalid_str = sslcertwindow_get_invalid_str(cert);
	const gchar *title;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	buf = g_strdup_printf(_("Certificate for %s is expired.\n%sDo you want to continue?"), cert->host, invalid_str);
	g_free(invalid_str);

	label = gtk_label_new(buf);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
	g_free(buf);
	
	sig_status = ssl_certificate_check_signer(cert, cert->status);

	if (sig_status==NULL)
		sig_status = g_strdup(_("Correct"));

	buf = g_strdup_printf(_("Signature status: %s"), sig_status);
	label = gtk_label_new(buf);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
	g_free(buf);
	g_free(sig_status);
	
	button = gtk_expander_new_with_mnemonic(_("_View certificate"));
	gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
	cert_widget = cert_presenter(cert);
	gtk_container_add(GTK_CONTAINER(button), cert_widget);

	if (!ssl_certificate_check_subject_cn(cert))
		title = _("TLS certificate is invalid and expired");
	else
		title = _("TLS certificate is expired");

	val = alertpanel_full(title, NULL,
			      NULL, _("_Cancel connection"), NULL, _("_Accept"),
			      NULL, NULL, ALERTFOCUS_FIRST, FALSE, vbox, ALERT_QUESTION);
	
	return (val == G_ALERTALTERNATE);
}

static gboolean sslcertwindow_ask_changed_cert(SSLCertificate *old_cert, SSLCertificate *new_cert)
{
	GtkWidget *old_cert_widget = cert_presenter(old_cert);
	GtkWidget *new_cert_widget = cert_presenter(new_cert);
	GtkWidget *vbox;
	gchar *buf, *sig_status;
	GtkWidget *vbox2;
	GtkWidget *label;
	GtkWidget *button;
	AlertValue val;
	gchar *invalid_str = sslcertwindow_get_invalid_str(new_cert);
	const gchar *title;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	label = gtk_label_new(_("New certificate:"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_box_pack_end(GTK_BOX(vbox), new_cert_widget, TRUE, TRUE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), label, TRUE, TRUE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), TRUE, TRUE, 0);
	label = gtk_label_new(_("Known certificate:"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_box_pack_end(GTK_BOX(vbox), old_cert_widget, TRUE, TRUE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), label, TRUE, TRUE, 0);
	gtk_widget_show_all(vbox);
	
	vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	buf = g_strdup_printf(_("Certificate for %s has changed.\n%sDo you want to accept it?"), new_cert->host, invalid_str);
	g_free(invalid_str);

	label = gtk_label_new(buf);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_box_pack_start(GTK_BOX(vbox2), label, TRUE, TRUE, 0);
	g_free(buf);
	
	sig_status = ssl_certificate_check_signer(new_cert, new_cert->status);

	if (sig_status==NULL)
		sig_status = g_strdup(_("Correct"));

	buf = g_strdup_printf(_("Signature status: %s"), sig_status);
	label = gtk_label_new(buf);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_box_pack_start(GTK_BOX(vbox2), label, TRUE, TRUE, 0);
	g_free(buf);
	g_free(sig_status);
	
	button = gtk_expander_new_with_mnemonic(_("_View certificates"));
	gtk_box_pack_start(GTK_BOX(vbox2), button, FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(button), vbox);

	if (!ssl_certificate_check_subject_cn(new_cert))
		title = _("TLS certificate changed and is invalid");
	else
		title = _("TLS certificate changed");
	val = alertpanel_full(title, NULL,
			      NULL, _("_Cancel connection"), NULL, _("_Accept and save"),
			      NULL, NULL, ALERTFOCUS_FIRST, FALSE, vbox2, ALERT_WARNING);
	
	return (val == G_ALERTALTERNATE);
}
#endif
