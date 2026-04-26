#pragma once
#include <string>

// Override: make PREFIX=/opt/hspm LIVE=/usr DISTFILES=/usr/src/distfiles/

#ifndef HSPM_ROOT_PATH
#define HSPM_ROOT_PATH "/opt/hspm"
#endif

#ifndef HSPM_LIVE_PATH
#define HSPM_LIVE_PATH "/usr"
#endif

#ifndef HSPM_DISTFILES_PATH
#define HSPM_DISTFILES_PATH "/usr/src/distfiles/"
#endif

static const std::string HSPM_ROOT      = HSPM_ROOT_PATH;
static const std::string HSPM_STORE     = HSPM_ROOT + "/store/";
static const std::string HSPM_DB        = HSPM_ROOT + "/db/hspm.db";
static const std::string HSPM_LOG       = HSPM_ROOT + "/logs/hspm.log";
static const std::string HSPM_RECIPES   = HSPM_ROOT + "/recipes/";
static const std::string HSPM_BUILDERS  = HSPM_ROOT + "/builders/";
static const std::string HSPM_URLS      = HSPM_ROOT + "/blfs-urls.txt";
static const std::string HSPM_SCHEMA    = HSPM_ROOT + "/db/schema.sql";
static const std::string HSPM_SCRAPER   = HSPM_ROOT + "/tools/blfs-scraper.py";
static const std::string HSPM_BUILD     = "/tmp/hspm-build/";
static const std::string HSPM_LIVE      = HSPM_LIVE_PATH;
static const std::string HSPM_DISTFILES = HSPM_DISTFILES_PATH;
