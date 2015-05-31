////////////////////////////////////////////////////////////////////////////////
//               Copyright(c) 2013 SDU, All right reserved
// Filename        :    dmwrapper.cpp
// Projectname     :    RenderingUnit
// Author          :    Jujj
// Email           :    
// Date            :    2013-07-24
// Version         :    v 1.0
// Description     :    
////////////////////////////////////////////////////////////////////////////////
#include "rUnit.h"
#include "task.h"
#include "dmwrapper.h"

#ifdef HAVE_DMCS
#include "mystdio.h"
#endif

// void init_dmf_master(int, char *, int, int)
// {
// }

// void init_dmf_slave(int)
// {
// }

void startDMMaster()
{
#ifdef HAVE_DMCS
#ifdef SDU_DM
	dm_init_master(gl_cfg.repli);
#else
	DEBUG_PRINT("DM master starting......\n");
	init_dmf_master(gl_cfg.commSize - 1, 0);
#endif
#endif
}

void startDMSlave()
{
#ifdef HAVE_DMCS
#ifdef SDU_DM
	dm_init_slave(gl_cfg.sharedSpace * 1048576, gl_cfg.repli);
#else
	char sharePath[MAX_PATH_LEN];
	sprintf(sharePath, "%s/%s", gl_cfg.ribPath, "Shaders");
	DEBUG_PRINT("DM slave started with shared mem %dM\n", gl_cfg.sharedSpace);
	init_dmf_slave(gl_cfg.sharedSpace * 1048576);
#endif
#endif
}
void dmLoadSharedFiles(LenJob *job)
{
#ifdef HAVE_DMCS
#ifdef SDU_DM
	char sharePath[MAX_PATH_LEN];
	sprintf(sharePath, "%s/%s", job->lenPath, "Textures");
	dm_loadfiles(sharePath);
#else
	char sharePath[MAX_PATH_LEN];
	sprintf(sharePath, "%s/%s:", job->lenPath, "Textures");
	DEBUG_PRINT("Loading shared files with path %s\n", sharePath);
	dmf_load_files(sharePath);
#endif
#endif
}
void dmFinish()
{
#ifdef HAVE_DMCS
#ifdef SDU_DM
	dm_finish();
#else
#endif
#endif
}
