// CollisionMap.cpp: implementation of the CCollisionMap class.
//
//////////////////////////////////////////////////////////////////////
#include "D2Map.h"
#include <string>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CCollisionMap::CCollisionMap()
{
	m_iCurMap = 0x00;
	::memset(&m_ptLevelOrigin, 0, sizeof(POINT));
}

CCollisionMap::~CCollisionMap()
{
}

void CCollisionMap::OnMapChanged(BYTE iNewMapID)
{
	if (iNewMapID != m_iCurMap)
	{
		m_iCurMap = iNewMapID;
		m_map.Lock();
		m_map.Destroy();
		//m_map.Unlock();	
		::memset(&m_ptLevelOrigin, 0, sizeof(POINT));
		m_aCollisionTypes.RemoveAll();
	}
}

void CCollisionMap::AddCollisionData(const CollMap* pCol)
{
	if (pCol == NULL)
		return;

	int x = pCol->dwPosGameX - m_ptLevelOrigin.x;
	int y = pCol->dwPosGameY - m_ptLevelOrigin.y;
	int cx = pCol->dwSizeGameX;
	int cy = pCol->dwSizeGameY;

	if (!m_map.IsValidIndex(x, y))
	{
		return;
	}	
	
	int nLimitX = x + cx;
	int nLimitY = y + cy;
	
	WORD* p = pCol->pMapStart;
	for (int j = y; j < nLimitY; j++)		
	{
		for (int i = x; i < nLimitX; i++)
		{
			
			m_map[i][j] = *p;
			if (m_map[i][j] == 1024)
				m_map[i][j] = MAP_DATA_AVOID;

			if (m_aCollisionTypes.Find(*p) == -1)
			{
				m_aCollisionTypes.Add(*p);
				m_aCollisionTypes.Sort();
			}

			p++;
		}
	}
}

BOOL CCollisionMap::IsValidAbsLocation(long x, long y) const
{
	if (!m_map.IsCreated())
		return FALSE;

	x -= m_ptLevelOrigin.x;
	y -= m_ptLevelOrigin.y;
	return m_map.IsValidIndex(x, y);
}

WORD CCollisionMap::GetMapData(long x, long y, BOOL bAbs) const
{
	if (!m_map.IsCreated())
		return MAP_DATA_INVALID;
	
	if (bAbs)
	{
		x -= m_ptLevelOrigin.x;
		y -= m_ptLevelOrigin.y;
	}

	//m_map.Lock();
	WORD wVal = MAP_DATA_INVALID;

	if (m_map.IsValidIndex(x, y))
		wVal = m_map[x][y];

	//m_map.Unlock();
	return wVal;
}


BOOL CCollisionMap::BuildMapData(DWORD AreaId)
{
	if (m_map.IsCreated())
		return TRUE;


	Level *pLevel = GetLevel(pAct->pMisc,AreaId);
	if(!pLevel)
		return FALSE;	

	dwLevelId = AreaId;
	
	m_ptLevelOrigin.x = pLevel->dwPosX * 5;
	m_ptLevelOrigin.y = pLevel->dwPosY * 5;
	
	//m_map.Lock();
	if (!m_map.Create(pLevel->dwSizeX * 5, pLevel->dwSizeY * 5, MAP_DATA_INVALID))
	{
		//m_map.Unlock();
		return FALSE;
	}

	//DwordArray aSkip;
	//Search(pLevel->pRoom2First, NULL, aSkip);

	for(Room2* pRoom2 = pLevel->pRoom2First; pRoom2; pRoom2 = pRoom2->pRoom2Next)
	{
		BOOL bAdded = FALSE;

		if(!pRoom2->pRoom1)
		{
			bAdded = TRUE;
			D2COMMON_AddRoomData(pAct, pLevel->dwLevelNo, pRoom2->dwPosX, pRoom2->dwPosY, NULL);
		}

		if(pRoom2->pRoom1)
			AddCollisionData(pRoom2->pRoom1->Coll);

		if(bAdded)
			D2COMMON_RemoveRoomData(pAct, pLevel->dwLevelNo, pRoom2->dwPosX, pRoom2->dwPosY, NULL);

	}

	// Fill gaps
	FillGaps();
	FillGaps(); // Yes, we need to do it twice
	//ThickenWalls(m_map, 1);
	
	//m_map.Unlock();
	return TRUE;
}

BOOL CCollisionMap::CreateMap(Act* lpAct,DWORD AreaId)
{
	CCollisionMap::pAct = lpAct;	
	BOOL bOK = BuildMapData(AreaId);

	return bOK;
}

POINT CCollisionMap::GetMapOrigin() const
{
	return m_ptLevelOrigin;
}

void CCollisionMap::AbsToRelative(POINT &pt) const
{
	pt.x -= m_ptLevelOrigin.x;
	pt.y -= m_ptLevelOrigin.y;
}

void CCollisionMap::RelativeToAbs(POINT &pt) const
{
	pt.x += m_ptLevelOrigin.x;
	pt.y += m_ptLevelOrigin.y;
}

BOOL CCollisionMap::Export(char* map)
{
	const int CX = m_map.GetCX();
	const int CY = m_map.GetCY();

	/*map = new char[CX*CY + 500];

	for (int i = 0; i < 0xdb24; i++)
	{
		map[i] = 0; 
	}*/

	int length = sprintf(map, "%d,%d\n", m_ptLevelOrigin.x, m_ptLevelOrigin.y); 

	for (int y = 0; y < CY; y++)
	{		
		for (int x = 0; x < CX; x++)
		{
			if (!m_map.IsValidIndex(x, y))
				continue;
			
			int value = m_map[x][y];

			map[length] =  (value % 2) ? 'X' : ' ';		
			length++;
		}
		map[length] = '\n';
		length++;
	}
	
	map[length] = '\0';
	return true; 
}

BOOL CCollisionMap::DumpMap(LPCSTR lpszFilePath, const LPPOINT lpPath, DWORD dwCount) const
{
	if (lpszFilePath == NULL)
		return FALSE;

	FILE *fp = fopen(lpszFilePath, "w+");
	if(fp == NULL )
		return FALSE;	
	
	if (!m_map.IsCreated())
		return FALSE;

	//m_map.Lock();
	char szMapName[256] = "";

	fprintf(fp, "%d,%d\n", m_ptLevelOrigin.x, m_ptLevelOrigin.y);
	//for (int i = 0; i < m_aCollisionTypes.GetSize(); i++)
	//{
	//	fprintf(fp, "%d, ", m_aCollisionTypes[i]);
	//}

	//fprintf(fp, "\n\n");

	POINT* pPath = NULL;
	if (lpPath && dwCount)
	{
		pPath = new POINT[dwCount];
		::memcpy(pPath, lpPath, dwCount * sizeof(POINT));
		for (DWORD i = 0; i < dwCount; i++)
			AbsToRelative(pPath[i]);
	}	

	const int CX = m_map.GetCX();
	const int CY = m_map.GetCY();

	for (int y = 0; y < CY; y++)
	{		
		for (int x = 0; x < CX; x++)
		{
			char ch = IsMarkPoint(NULL, x, y, pPath, dwCount);			

			if (!ch)
				ch = (m_map[x][y] % 2) ? 'X' : ' ';

			fprintf(fp, "%C", ch); // X - unreachable
		}

		fprintf(fp, "%c", '\n');
	}

	if (pPath)
		delete [] pPath;

	//m_map.Unlock();	
	fclose(fp);

	return TRUE;
}

BOOL CCollisionMap::CheckCollision(INT x, INT y) {
	if(!m_map.IsCreated())
		return FALSE;
	if(x > m_map.GetCX() || y > m_map.GetCY())
		return FALSE;
	BOOL Works = FALSE;
	Works = (m_map[x][y] % 2) ? FALSE : TRUE;
return Works;
}

char CCollisionMap::IsMarkPoint(INT asd, int x, int y, const LPPOINT lpPath, DWORD dwCount) const
{	
	char ch = 0;

	if (lpPath == NULL || dwCount == 0)
		return ch;

	for (DWORD i = 0; i < dwCount; i++)
	{
		if (lpPath[i].x == x && lpPath[i].y == y)
		{
			if (i == 0)
				return 'S'; // start
			else if (i == dwCount - 1)
				return 'E'; // end
			else
				return '*'; // nodes
		}
	}

	return ch;
}

SIZE CCollisionMap::GetMapSize() const
{
	SIZE cz = {0};
	if (m_map.IsCreated())
	{
		cz.cx = m_map.GetCX();
		cz.cy = m_map.GetCY();
	}
	return cz;
}

SIZE CCollisionMap::CopyMapData(WORD** ppBuffer, int cx, int cy) const
{
	SIZE copied = {0};
	//m_map.Lock();
	copied = m_map.ExportData(ppBuffer, cx, cy);
	//m_map.Unlock();
	return copied;
}

BOOL CCollisionMap::IsGap(int x, int y) 
{
	if (m_map[x][y] % 2)
		return FALSE;

	int nSpaces = 0;
	int i = 0;

	// Horizontal check
	for (i = x - 2; i <= x + 2 && nSpaces < 3; i++)
	{
		if ( i < 0 || i >= m_map.GetCX() || (m_map[i][y] % 2))
			nSpaces = 0;
		else
			nSpaces++;
	}

	if (nSpaces < 3)
		return TRUE;

	// Vertical check
	nSpaces = 0;
	for (i = y - 2; i <= y + 2 && nSpaces < 3; i++)
	{
		if ( i < 0 || i >= m_map.GetCY() || (m_map[x][i] % 2))
			nSpaces = 0;
		else
			nSpaces++;
	}

	return nSpaces < 3;
}

void CCollisionMap::FillGaps()
{
	if (!m_map.IsCreated())
		return;

	//m_map.Lock();

	const int CX = m_map.GetCX();
	const int CY = m_map.GetCY();

	for (int x = 0; x <CX; x++)
	{
		for (int y = 0; y < CY; y++)
		{
			if (IsGap(x, y))
				m_map[x][y] = MAP_DATA_FILLED;
		}
	}

	//m_map.Unlock();
}

void CCollisionMap::MakeBlank(WordMatrix& rMatrix, POINT pos)
{
	if (!rMatrix.IsCreated())
		return;

	for (int i = pos.x - 1; i <= pos.x + 1; i++)
	{
		for (int j = pos.y - 1; j <= pos.y + 1; j++)
		{
			if (rMatrix.IsValidIndex(i, j))
				rMatrix[i][j] = MAP_DATA_CLEANED;
		}
	}
}

BOOL CCollisionMap::ThickenWalls(WordMatrix& rMatrix, int nThickenBy)
{
	if (!rMatrix.IsCreated() || nThickenBy <= 0)
		return FALSE;

	const int CX = rMatrix.GetCX();
	const int CY = rMatrix.GetCY();
	
	for (int i = 0; i < CX; i++)
	{
		for (int j = 0; j < CY; j++)
		{
			if ((rMatrix[i][j] % 2) == 0 || rMatrix[i][j] == MAP_DATA_THICKENED)
				continue;

			for (int x = i - nThickenBy; x <= i + nThickenBy; x++)
			{
				for (int y = j - nThickenBy; y <= j + nThickenBy; y++)
				{
					if (!rMatrix.IsValidIndex(x, y))
						continue;

					if ((rMatrix[x][y] % 2) == 0)
						rMatrix[x][y] = MAP_DATA_THICKENED;
				}
			}
		}
	}

	return TRUE;
}

void CCollisionMap::DestroyMap()
{
	//m_map.Lock();
	m_map.Destroy();
	//m_map.Unlock();
	m_iCurMap = 0x00;
	m_aCollisionTypes.RemoveAll();
	::memset(&m_ptLevelOrigin, 0, sizeof(POINT));
}

BOOL CCollisionMap::CopyMapData(WordMatrix& rBuffer) const
{
	//m_map.Lock();
	m_map.ExportData(rBuffer);
	//m_map.Unlock();
	return rBuffer.IsCreated();
}

BOOL CCollisionMap::ReportCollisionType(POINT ptOrigin, long lRadius) const
{
	if (!m_map.IsCreated() || lRadius < 1)
		return FALSE;

	//m_map.Lock();
	WordArray aList;
	AbsToRelative(ptOrigin);
	for (int i = ptOrigin.x - lRadius; i <= ptOrigin.x + lRadius; i++)
	{
		for (int j = ptOrigin.y - lRadius; j <= ptOrigin.y + lRadius; j++)
		{
			if (!m_map.IsValidIndex(i, j))
				continue;

			if (aList.Find(m_map[i][j]) == -1)
				aList.Add(m_map[i][j]);
		}
	}

	//m_map.Unlock();
	aList.Sort();

//	char sz[256] = "";
	//for (int n = 0; n < aList.GetSize(); n++)
	//	sprintf(sz, "%s%d, ", sz, aList[n]);

	
	return aList.GetSize();
}

INT CCollisionMap::GetLevelExits(LPLevelExit* lpLevel)
{
	POINT ptExitPoints[0x40][2];
	INT nTotalPoints = 0, nCurrentExit = 0;
	INT nMaxExits = 0x40;
	UnitAny* Me = D2CLIENT_GetPlayerUnit();

	for(INT i = 0; i < m_map.GetCX(); i++)
	{
		if(!(m_map[i][0] % 2))
		{
			ptExitPoints[nTotalPoints][0].x = i;
			ptExitPoints[nTotalPoints][0].y = 0;

			for(i++; i < m_map.GetCX(); i++)
			{
				if(m_map[i][0] % 2)
				{
					ptExitPoints[nTotalPoints][1].x = i - 1;
					ptExitPoints[nTotalPoints][1].y = 0;
					break;
				}
			}

			nTotalPoints++;
			break;
		}
	}

	for(INT i = 0; i < m_map.GetCX(); i++)
	{
		if(!(m_map[i][m_map.GetCY() - 1] % 2))
		{
			ptExitPoints[nTotalPoints][0].x = i;
			ptExitPoints[nTotalPoints][0].y = m_map.GetCY() - 1;

			for(i++; i < m_map.GetCX(); i++)
			{
				if((m_map[i][m_map.GetCY() - 1] % 2))
				{
					ptExitPoints[nTotalPoints][1].x = i - 1;
					ptExitPoints[nTotalPoints][1].y = m_map.GetCY() - 1;
					break;
				}
			}

			nTotalPoints++;
			break;
		}
	}

	for(INT i = 0; i < m_map.GetCY(); i++)
	{
		if(!(m_map[0][i] % 2))
		{
			ptExitPoints[nTotalPoints][0].x = 0;
			ptExitPoints[nTotalPoints][0].y = i;

			for(i++; i < m_map.GetCY(); i++)
			{
				if((m_map[0][i] % 2))
				{
					ptExitPoints[nTotalPoints][1].x = 0;
					ptExitPoints[nTotalPoints][1].y = i - 1;
					break;
				}
			}

			nTotalPoints++;
			break;
		}
	}

	for(INT i = 0; i < m_map.GetCY(); i++)
	{
		if(!(m_map[m_map.GetCX() - 1][i] % 2))
		{
			ptExitPoints[nTotalPoints][0].x = m_map.GetCX() - 1;
			ptExitPoints[nTotalPoints][0].y = i;

			for(i++; i < m_map.GetCY(); i++)
			{
				if((m_map[m_map.GetCX() - 1][i] % 2))
				{
					ptExitPoints[nTotalPoints][1].x = m_map.GetCX() - 1;
					ptExitPoints[nTotalPoints][1].y = i - 1;
					break;
				}
			}

			nTotalPoints++;
			break;
		}
	}

	LPPOINT ptCenters = new POINT[nTotalPoints];
	for(INT i = 0; i < nTotalPoints; i++)
	{
		INT nXDiff = ptExitPoints[i][1].x - ptExitPoints[i][0].x;
		INT nYDiff = ptExitPoints[i][1].y - ptExitPoints[i][0].y;
		INT nXCenter = 0, nYCenter = 0;

		if(nXDiff > 0)
		{
			if(nXDiff % 2)
				nXCenter = ptExitPoints[i][0].x + ((nXDiff - (nXDiff % 2)) / 2);
			else
				nXCenter = ptExitPoints[i][0].x + (nXDiff / 2);
		}

		if(nYDiff > 0)
		{
			if(nYDiff % 2)
				nYCenter = ptExitPoints[i][0].y + ((nYDiff - (nYDiff % 2)) / 2);
			else
				nYCenter = ptExitPoints[i][0].y + (nYDiff / 2);
		}

		ptCenters[i].x = nXCenter ? nXCenter : ptExitPoints[i][0].x;
		ptCenters[i].y = nYCenter ? nYCenter : ptExitPoints[i][0].y;
	}

	

	for(Room2* pRoom = GetLevel(D2CLIENT_GetPlayerUnit()->pAct->pMisc, dwLevelId)->pRoom2First; pRoom; pRoom = pRoom->pRoom2Next)
	{
		Room2* *pNear = pRoom->pRoom2Near;
		for(DWORD i = 0; i < pRoom->dwRoomsNear; i++)
		{
			if(pNear[i]->pLevel->dwLevelNo != dwLevelId)
			{
				INT nRoomX = pRoom->dwPosX * 5;
				INT nRoomY = pRoom->dwPosY * 5;

				for(INT j = 0; j < nTotalPoints; j++)
				{
					if((ptCenters[j].x + m_ptLevelOrigin.x) >= (WORD)nRoomX && (ptCenters[j].x + m_ptLevelOrigin.x) <= (WORD)(nRoomX + (pRoom->dwSizeX * 5)))
					{
						if((ptCenters[j].y + m_ptLevelOrigin.y) >= (WORD)nRoomY && (ptCenters[j].y + m_ptLevelOrigin.y) <= (WORD)(nRoomY + (pRoom->dwSizeY * 5)))
						{
							if(nCurrentExit >= nMaxExits)
								return FALSE;

							lpLevel[nCurrentExit] = new LevelExit;
							lpLevel[nCurrentExit]->dwTargetLevel = pNear[i]->pLevel->dwLevelNo;
							lpLevel[nCurrentExit]->ptPos.x = ptCenters[j].x + m_ptLevelOrigin.x;
							lpLevel[nCurrentExit]->ptPos.y = ptCenters[j].y + m_ptLevelOrigin.y;
							nCurrentExit++;
						}
					}
				}

				break;
			}
		}
	}

	return nCurrentExit;
}
