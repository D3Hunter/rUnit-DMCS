////////////////////////////////////////////////////////////////////////////////
//               Copyright(c) 2013 SDU, All right reserved
// Filename        :    dmwrapper.h
// Projectname     :    RenderingUnit
// Author          :    Jujj
// Email           :    
// Date            :    2013-07-24
// Version         :    v 1.0
// Description     :    
////////////////////////////////////////////////////////////////////////////////
#ifndef __DMWRAPPER_H__
#define __DMWRAPPER_H__

void startDMMaster();
void startDMSlave();
void dmLoadSharedFiles(LenJob *job);
void dmFinish();

#endif
