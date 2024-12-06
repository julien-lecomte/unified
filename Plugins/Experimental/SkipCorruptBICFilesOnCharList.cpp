#include "nwnx.hpp"
#include "API/CAppManager.hpp"
#include "API/CServerExoApp.hpp"
#include "API/CServerExoAppInternal.hpp"
#include "API/CNetLayer.hpp"
#include "API/CNetLayerPlayerInfo.hpp"
#include "API/CNWSPlayer.hpp"
#include "API/CServerInfo.hpp"
#include "API/CResList.hpp"
#include "API/CExoBase.hpp"
#include "API/CExoResMan.hpp"
#include "API/CResGFF.hpp"
#include "API/CExoArrayList.hpp"
#include "API/NWPlayerCharacterList_st.hpp"
#include "API/NWPlayerCharacterListClass_st.hpp"

namespace Experimental {

using namespace NWNXLib;
using namespace NWNXLib::API;
using namespace NWNXLib::API::Constants;

void SkipCorruptBICFilesOnCharList() __attribute__((constructor));
void SkipCorruptBICFilesOnCharList()
{
    if (!Config::Get<bool>("SKIP_CORRUPT_BIC_FILES_ON_CHAR_LIST", false))
        return;

    LOG_INFO("EXPERIMENTAL: Skipping corrupt BIC files when sending character list.");

    static Hooks::Hook s_SendServerToPlayerCharListHook = Hooks::HookFunction(
    &CNWSMessage::SendServerToPlayerCharList,
    +[](CNWSMessage *pThis, CNWSPlayer* pPlayer) -> int32_t
    {
        auto *pPlayerInfo = Globals::AppManager()->m_pServerExoApp->m_pcExoAppInternal->m_pNetLayer->GetPlayerInfo(pPlayer->m_nPlayerID);
        uint32_t nMessageSize = sizeof(uint16_t);
        pThis->CreateWriteMessage(nMessageSize, pPlayer->m_nPlayerID);

        if (Globals::AppManager()->m_pServerExoApp->GetServerInfo()->m_JoiningRestrictions.bAllowLocalVaultChars ||
            (pPlayerInfo->m_bPlayerInUse && pPlayerInfo->m_bGameMasterPrivileges))
        {
            pThis->WriteWORD(0);
        }
        else
        {
            CExoString sSubDirectory = pPlayerInfo->m_cCDKey.sPublic.CStr();
            if (Globals::AppManager()->m_pServerExoApp->GetServerInfo()->m_PersistantWorldOptions.bServerVaultByPlayerName)
            {
                sSubDirectory = pPlayer->GetPlayerName();
            }

            CExoArrayList<CExoString> lstFilenames;
            CExoArrayList<NWPlayerCharacterList_st*> lstChars;
            lstFilenames.SetSize(0);
            CExoString sDirectory;
            sDirectory.Format("SERVERVAULT:%s", sSubDirectory.CStr());
            Globals::ExoBase()->GetDirectoryList(&lstFilenames, sDirectory, Constants::ResRefType::BIC);
            if (lstFilenames.num)
            {
                Globals::ExoResMan()->AddResourceDirectory(sDirectory, 81 * 1000000);

                for (int i = 0; i < lstFilenames.num; i++)
                {
                    CExoString sFile = lstFilenames[i];
                    sFile = sFile.SubString(0, sFile.GetLength() - 4);
                    auto *pRes = new CResGFF(ResRefType::BIC, "BIC ", sFile);
                    if (pRes->m_bLoaded)
                    {
                        if (pRes->m_bValidationFailed)
                        {
                            LOG_WARNING("CORRUPT BIC FILE IN VAULT OF PLAYER: '%s (%s)', FILENAME: '%s.bic', SKIPPING", pPlayer->GetPlayerName().CStr(), pPlayerInfo->m_cCDKey.sPublic.CStr(), sFile);
                        }
                        else
                        {
                            CResStruct cTopLevelStruct{};
                            CResList cGffClassList{};
                            CResStruct cGffClassStruct{};
                            BOOL bSuccess;
                            auto *pEntry = new NWPlayerCharacterList_st;

                            pRes->GetTopLevelStruct(&cTopLevelStruct);
                            pEntry->sLocFirstName = pRes->ReadFieldCExoLocString(&cTopLevelStruct, "FirstName", bSuccess);
                            pEntry->sLocLastName = pRes->ReadFieldCExoLocString(&cTopLevelStruct, "LastName", bSuccess);
                            pEntry->resFileName = sFile;
                            pEntry->nType = 0x11;
                            pEntry->nPortraitId = pRes->ReadFieldWORD(&cTopLevelStruct, "PortraitId", bSuccess, 0xffff);
                            pEntry->resPortrait = pRes->ReadFieldCResRef(&cTopLevelStruct, "Portrait", bSuccess);
                            if (pRes->GetList(&cGffClassList, &cTopLevelStruct, "ClassList"))
                            {
                                uint32_t nClasses = pRes->GetListCount(&cGffClassList);
                                for (uint32_t nClass = 0; nClass < nClasses; nClass++)
                                {
                                    if (pRes->GetListElement(&cGffClassStruct, &cGffClassList, nClass))
                                    {
                                        NWPlayerCharacterListClass_st stClass{};
                                        stClass.nClass = pRes->ReadFieldINT(&cGffClassStruct, "Class", bSuccess, -1);
                                        stClass.nClassLevel = (uint8_t)pRes->ReadFieldSHORT(&cGffClassStruct, "ClassLevel", bSuccess, 0);
                                        pEntry->lstClasses.Add(stClass);
                                    }
                                }
                            }

                            if (!lstChars.DerefContains(pEntry))
                            {
                                if (!lstChars.AddUnique(pEntry))
                                {
                                    delete pEntry;
                                }
                            }
                            else
                            {
                                delete pEntry;
                            }
                        }
                    }
                    delete pRes;
                    pRes = nullptr;
                }
            }
            Globals::ExoResMan()->RemoveResourceDirectory(sDirectory);

            pThis->WriteWORD(lstChars.num);

            for (int32_t i = 0; i < lstChars.num; i++)
            {
                NWPlayerCharacterList_st *pEntry = lstChars[i];
                pThis->WriteCExoLocStringServer(pEntry->sLocFirstName);
                pThis->WriteCExoLocStringServer(pEntry->sLocLastName);
                pThis->WriteCResRef(pEntry->resFileName);
                pThis->WriteBYTE(pEntry->nType);
                pThis->WriteWORD(pEntry->nPortraitId);
                pThis-> WriteCResRef(pEntry->resPortrait);
                uint32_t nClasses = pEntry->lstClasses.num;
                pThis->WriteBYTE((uint8_t)nClasses);
                for (uint32_t nClass = 0; nClass < nClasses; nClass++)
                {
                    pThis->WriteINT(pEntry->lstClasses[nClass].nClass);
                    pThis->WriteBYTE(pEntry->lstClasses[nClass].nClassLevel);
                }
                delete pEntry;
            }

        }

        uint8_t* pMessage;
        if (!pThis->GetWriteMessage(&pMessage, &nMessageSize))
            return false;

        return pThis->SendServerToPlayerMessage(pPlayer->m_nPlayerID, Constants::MessageMajor::CharList, Constants::MessageCharListMinor::ListResponse, pMessage, nMessageSize);
    }, Hooks::Order::Final);
}

}
