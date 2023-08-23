#include "GXDSkin2Loader.h"
#include "GXDTextureLoader.h"

#include "FileLoader.h"
#include "../XSystem/IO/Zlib.h"

#include "../XEngine.h"
#include "../XSkinMesh.h"

using namespace XSystem::IO;

void DebugA(const char*,...)
{
}

/// <summary>
/// LoadSkin2Chunck
/// </summary>
namespace {

	bool LoadSkin2Texture(BinaryReader& br, SkinData2& s)
	{
		if (!s.mDiffuseMap.Load(br))
		{
			DebugA("<< !LoadSkin2Texture::mDiffuseMap()\r\n");
			return false;
		}

		if (!s.mNormalMap.Load(br))
		{
			DebugA("<< !LoadSkin2Texture::mNormalMap()\r\n");
			return false;
		}

		if (!s.mSpecularMap.Load(br))
		{
			DebugA("<< !LoadSkin2Texture::mSpecularMap()\r\n");
			return false;
		}

		int mAnimationNum = br.ReadInt();
		DebugA("<< !LoadSkin2Texture::mAnimationNum -> %d\r\n", mAnimationNum);
		if (mAnimationNum > 0)
		{
			s.mAnimationMap.resize(mAnimationNum);
			for (int i = 0; i < mAnimationNum; ++i)
				if (!s.mAnimationMap[i].Load(br))
					return false;
		}

		DebugA("<< LoadSkin2Texture()\r\n");

		return true;
	}

	bool LoadSkin2Vertex(BinaryReader& br, SkinData2& s)
	{
		int mLODStepNum = 0;
		if (!br.ReadBytes(&mLODStepNum, sizeof(mLODStepNum)))
		{
			DebugA("<< !LoadSkin2Vertex::mLODStepNum -> %d\r\n", mLODStepNum);
			return false;
		}

		DebugA("<< LoadSkin2Vertex::mLODStepNum -> %d\r\n", mLODStepNum);

		if (!mLODStepNum)
			return false;

		s.mLOD.resize( mLODStepNum );

		//auto device = &XEngine::Instance();
		for (int i = 0; i < mLODStepNum; ++i)
		{
			auto l = &s.mLOD[i];

			int mVertexNum = l->mVertexNum = br.ReadInt();
			int vtxSize = sizeof(SkinVertex2) * mVertexNum;
			auto vtx = &l->vertices;
			//void* vbuffer = NULL;
			//device->CreateVertexBuffer(vtxSize, D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &vtx, 0);
			//vtx->Lock(0, 0, &vbuffer, 0);
			//if (br.ReadBytes(vbuffer, vtxSize)) {
			//}
			//vtx->Unlock();
			vtx->resize( mVertexNum );
			br.ReadBytes( vtx->data(), vtxSize );


			int mIndexNum = l->mIndexNum = br.ReadInt();
			int idxSize = 6 * mIndexNum;
			auto idx = &l->indices;
			//void* ibuffer = NULL;
			//device->CreateIndexBuffer(idxSize, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_MANAGED, &idx, 0);
			//idx->Lock(0, 0, &ibuffer, 0);
			//if (br.ReadBytes(ibuffer, idxSize)) {
			//}
			//idx->Unlock();
			//br.ReadEmpty(idxSize);
			idx->resize(mIndexNum * 3);
			br.ReadBytes( idx->data(), idxSize );


			int shdSize = sizeof(SkinShadow2) * mVertexNum;

			//mShadowVertexBuffer
			br.ReadEmpty(shdSize);

			//mShadowIndexBuffer
			br.ReadEmpty(idxSize);

			//mShadowEdgeBuffer
			br.ReadEmpty(idxSize);
		}

		return LoadSkin2Texture(br, s);
	}

	bool LoadSkinData2(BinaryReader& br, SkinData2& s)
	{
		s.mCheckValidState = br.ReadInt() > 0;
		if (!s.mCheckValidState)
		{
			return true;
		}

		s.mCheckValidState = false;

		if (!br.ReadBytes(&s.mEffect, sizeof(s.mEffect)))
		{
			DebugA("<< !LoadSkin2Real::mEffect()\r\n");
			return false;
		}
		
		if (!br.ReadBytes(&s.mSize, sizeof(s.mSize)))
		{
			DebugA("<< !LoadSkin2Real::mSize()\r\n");
			return false;
		}
		
		if (!br.ReadBytes(s.mVertexBufferForBillboard, sizeof(s.mVertexBufferForBillboard)))
		{
			DebugA("<< !LoadSkin2Real::mVertexBufferForBillboard()\r\n");
			return false;
		}

		s.mCheckValidState = LoadSkin2Vertex(br, s);
		{
			DebugA("<< !LoadSkin2Real::LoadSkin2Vertex()\r\n");
		}

		return s.mCheckValidState;
	}
}

/// <summary>
/// LoadSkin2Base
/// </summary>
namespace {

	bool LoadSkin2Extra(BinaryReader& br, bool& tValid, bool& tCompressed)
	{
		char tBuffer[5];
		return br.ReadString(tBuffer, 4) && ( tValid = tBuffer[0] > 0, tCompressed = tBuffer[1] > 0 );
	}
	bool LoadSkin2Header(BinaryReader& br, int* tVersion)
	{
		char tBuffer[12];

		return br.ReadString(tBuffer, 8) && strncmp("SOBJECT", tBuffer, 7) == 0 && ( ( *tVersion = tBuffer[7] - '0' ), *tVersion == 2 || *tVersion == 3 );
	}

	bool LoadSkin2CompressChunk(SkinVersion2* s, BinaryReader& br)
	{
		int mSkinNum = 0;
		Zlib z( br );
		if (!Zlib::Decompress(z))
		{
			DebugA("<< !LoadSkin2CompressChunk::Decompress()\r\n");
			return false;
		}

		BinaryReader sub( &z.m_OriginalData, 0, z.m_OriginalSize );
		mSkinNum = sub.ReadInt();
		if (mSkinNum > 0)
		{
			bool res = false;

			s->mSkin.resize( mSkinNum );
			for (int i = 0; i < mSkinNum; i++)
			{
				res = LoadSkinData2(sub, s->mSkin[i]);
				if (!res)
					break;
			}

			DebugA("<< LoadSkin2CompressChunk::LoadSkin2Data() -> %d\r\n", res);

			if (!res) {
				delete s;
				s = nullptr;
			}

			return res;
		}


		return false;
	}

}

namespace XLoader {

	GXDSkin2Loader::GXDSkin2Loader()
	{
	}

	bool GXDSkin2Loader::Load(const char* tFileName, XSkinMesh* skin)
	{
		bool res = false;
		FileLoader f;
		std::vector<char> data;

		if (!f.Load(tFileName, data))
		{
			DebugA( "%s << size: %d\r\n", tFileName, data.size() );
			return false;
		}
		
		int tVersion;
		BinaryReader br( &data, 0, data.size() );
		if (!LoadSkin2Header(br, &tVersion))
		{
			DebugA("%s << !LoadSkin2Header()\r\n", tFileName);
			return res;
		}

		switch ( tVersion )
		{
		case 3:
			break;
		default:
			return res;
		}

		bool tValid, tCompressed;
		if (!LoadSkin2Extra(br, tValid, tCompressed))
		{
			DebugA("%s << !LoadSkin2Extra()\r\n", tFileName);
			return res;
		}

		if (!tValid)
			return true;

		if (tCompressed) {
			skin->v2 = new SkinVersion2();
			res = LoadSkin2CompressChunk(skin->v2, br);
			skin->Create2( res );
		}

		return res;
	}

}