// xImaLyr.cpp : Layers functions
/* 21/04/2003 v1.00 - Davide Pizzolato - www.xdp.it
 * CxImage version 5.99c 17/Oct/2004
 */

#include "ximage.h"

#if CXIMAGE_SUPPORT_LAYERS

////////////////////////////////////////////////////////////////////////////////
/**
 * If the object is an internal layer, GetParent return its parent in the hierarchy.
 */
CxImage* CxImage::GetParent() const
{
	return info.pParent;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Number of layers allocated directly by the object.
 */
long CxImage::GetNumLayers() const
{
	return info.nNumLayers;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Creates an empty layer. If position is less than 0, the new layer will be placed in the last position
 */
bool CxImage::LayerCreate(long position)
{
	if ( position < 0 || position > info.nNumLayers ) position = info.nNumLayers;

	CxImage** ptmp = (CxImage**)malloc((info.nNumLayers + 1)*sizeof(CxImage**));
	if (ptmp==0) return false;

	int i=0;
	for (int n=0; n<info.nNumLayers; n++){
		if (position == n){
			ptmp[n] = new CxImage();
			i=1;
		}
		ptmp[n+i]=pLayers[n];
	}
	if (i==0) ptmp[info.nNumLayers] = new CxImage();

	if (ptmp[position]){
		ptmp[position]->info.pParent = this;
	} else {
		free(ptmp);
		return false;
	}

	info.nNumLayers++;
	if (pLayers) free(pLayers);
	pLayers = ptmp;
	return true;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Deletes a layer. If position is less than 0, the last layer will be deleted
 */
bool CxImage::LayerDelete(long position)
{
	if ( position >= info.nNumLayers ) return false;
	if ( position < 0) position = info.nNumLayers - 1;

	CxImage** ptmp = (CxImage**)malloc((info.nNumLayers - 1)*sizeof(CxImage**));
	if (ptmp==0) return false;

	int i=0;
	for (int n=0; n<(info.nNumLayers - 1); n++){
		if (position == n){
			delete pLayers[n];
			i=1;
		}
		ptmp[n]=pLayers[n+i];
	}
	if (i==0) delete pLayers[info.nNumLayers - 1];

	info.nNumLayers--;
	if (pLayers) free(pLayers);
	pLayers = ptmp;
	return true;
}
////////////////////////////////////////////////////////////////////////////////
void CxImage::LayerDeleteAll()
{
	if (pLayers) { 
		for(long n=0; n<info.nNumLayers;n++){ delete pLayers[n]; }
		free(pLayers); pLayers=0;
	}
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Returns a pointer to a layer. If position is less than 0, the last layer will be returned
 */
CxImage* CxImage::GetLayer(long position)
{
	if ( position >= info.nNumLayers ) return 0;
	if ( position < 0) position = info.nNumLayers - 1;
	return pLayers[position];
}
////////////////////////////////////////////////////////////////////////////////
#endif //CXIMAGE_SUPPORT_LAYERS
