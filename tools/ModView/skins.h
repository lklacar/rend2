// Filename:-	skins.h
//

#ifndef SKINS_H
#define SKINS_H

/*
class CAlternativeMaterial
{
protected:
	string m_sName;

public:
	CSkinMaterialShader(const char * psName) {m_sName=psName;}
	~CSkinMaterialShader()			{ OutputDebugString( va("~CSkinMaterialShader(): %s\n",m_sName.c_str()) ); }
};
*/
typedef struct
{
	StringVector_t	vSurfacesOn;
	StringVector_t	vSurfacesOff;
	StringVector_t	vSurfacesOffNoChildren;

} SurfaceOnOffPrefs_t;

typedef std::vector<std::string/*CAlternativeMaterial*/> AlternativeShaders_t;		// each string = shader name
typedef std::map<std::string,AlternativeShaders_t> ShadersForMaterial_t;	// map key = (eg) "face", entry = alternative shader list
typedef std::map<std::string,ShadersForMaterial_t> SkinSet_t;				// map key = (eg) "white",entry = body component
typedef std::map<std::string,SkinSet_t> SkinSets_t;						// map key = (eg) "thug",entry = skin set

typedef std::map<std::string,SurfaceOnOffPrefs_t> SkinSetsSurfacePrefs_t;

bool Skins_ApplyEthnic	( ModelContainer_t *pContainer, const char * psSkin, const char * psEthnic, bool bApplySurfacePrefs, bool bDefaultSurfaces);
bool Skins_ApplySkinShaderVariant(ModelContainer_t *pContainer, const char * psSkin, const char * psEthnic, const char * psMaterial, int iVariant );
bool Skins_Validate		( ModelContainer_t *pContainer, int iSkinNumber );
bool Skins_ApplySkinFile(ModelContainer_t *pContainer, const std::string& strSkinFile, const std::string& strEthnic, bool bApplySurfacePrefs, bool bDefaultSurfaces, const std::string strMaterial = "", int iVariant = -1);
bool Skins_FilesExist	(const char * psModelFilename);
bool Skins_Read			(const char * psModelFilename, ModelContainer_t *pContainer);
//bool Skins_ApplyToTree	(HTREEITEM hTreeItem_Parent, ModelContainer_t *pContainer);
void Skins_ApplyDefault	(ModelContainer_t *pContainer);
bool Skins_FileHasSurfacePrefs(ModelContainer_t *pContainer, const char * psSkin);
void Skins_KillPreCacheInfo(void);
GLuint AnySkin_GetGLBind( ModelHandle_t hModel, const char * psMaterialName, const char * psSurfaceName );


#endif	// #ifndef SKINS_H

//////////////////// eof //////////////////////

