﻿// ViewSplit.cpp : DLL アプリケーション用にエクスポートされる関数を定義します。
//

#include "stdafx.h"
#include "../MMDUtility/mmdplugin/mmd_plugin.h"
#include "../MMDUtility/mmd/MMDExport.h"
#include "../MMDUtility/mmd_utility.h"


class ViewSplit : public MMDPluginDLL3
{
public:
  explicit ViewSplit(IDirect3DDevice9* device): device_(device) {}

  bool is_split_ = false;
  bool is_camera_right_bottom = false;

  void start() override
  {
    auto utility_dll = mmp::getDLL3Object("MMDUtility");
    if ( utility_dll == nullptr ) return;
    auto utility = dynamic_cast<MMDUtility*>(utility_dll);
    if ( utility == nullptr ) return;

    auto menu = utility->getUitilityMenu();
    auto ctrl = utility->getControl();
    auto check_view_split = new control::MenuCheckBox(ctrl);
    check_view_split->command = [check_view_split,this](const control::IMenu::CommandArgs&)
      {
        check_view_split->reverseCheck();
        is_split_ = check_view_split->isChecked();
      };
    menu->AppendChild(L"画面分割", check_view_split);

    auto check_camera_pos = new control::MenuCheckBox(ctrl);
    check_camera_pos->command = [check_camera_pos, this](const control::IMenu::CommandArgs&)
      {
        check_camera_pos->reverseCheck();
        is_camera_right_bottom = check_camera_pos->isChecked();
      };
    menu->AppendChild(L"カメラを右下にする", check_camera_pos);

    //check_view_split->Release();
    menu->AppendSeparator();
    //menu->Release();
  }

  static DirectX::XMMATRIX toMatrix(D3DMATRIX mat)
  {
    DirectX::XMMATRIX res;
    res = DirectX::XMLoadFloat4x4((DirectX::XMFLOAT4X4*)mat.m);
    return res;
  }

  static D3DMATRIX toMatrix(DirectX::XMMATRIX mat)
  {
    alignas(16) D3DMATRIX res;
    DirectX::XMStoreFloat4x4A((DirectX::XMFLOAT4X4A*)res.m, mat);
    return res;
  }

  void DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount) override
  {
    static int now_draw = 0;
    if ( now_draw ) return;

    device_->GetViewport(&back_viewport_);
    if ( is_split_ == false ) return;

    now_draw = 1;
    D3DMATRIX dworld, dview;
    device_->GetTransform(D3DTS_WORLD, &dworld);
    device_->GetTransform(D3DTS_VIEW, &dview);

    auto DrawView = [=](D3DVIEWPORT9 viewport, DirectX::XMMATRIX worldx)
      {
        // TODO: シェーダに渡されているデータもできれば変えて、再生時も対応できるようにする。
        auto tmp = toMatrix(worldx);
        device_->SetTransform(D3DTS_WORLD, &tmp);
        device_->SetViewport(&viewport);
        device_->DrawIndexedPrimitive(PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
      };
    auto Draw = [=](D3DMATRIX /*center*/)
      {
        D3DVIEWPORT9 viewport = back_viewport_;
        viewport.Width = viewport.Width / 2;
        viewport.Height = viewport.Height / 2;
        if ( depth_buf[0] == nullptr )
        {
          device_->GetDepthStencilSurface(&tmp_depth);
          if ( tmp_depth )
          {
            D3DSURFACE_DESC desc;
            tmp_depth->GetDesc(&desc);
            for ( auto& i : depth_buf )
            {
              device_->CreateDepthStencilSurface(desc.Width, desc.Height, desc.Format, desc.MultiSampleType, desc.MultiSampleQuality,
                                                 TRUE, &i, nullptr);
            }
          }
        }
        device_->SetDepthStencilSurface(depth_buf[0]);
        DrawView(viewport, DirectX::XMMatrixIdentity());

        viewport.X += viewport.Width;
        device_->SetDepthStencilSurface(depth_buf[1]);
        DrawView(viewport, DirectX::XMMatrixRotationY(DirectX::XM_PI / 2));

        viewport.X -= viewport.Width;
        viewport.Y += viewport.Height;
        device_->SetDepthStencilSurface(depth_buf[2]);
        DrawView(viewport, DirectX::XMMatrixRotationY(-DirectX::XM_PI / 2));
        if ( is_camera_right_bottom )
        {
          viewport.X += viewport.Width;
          device_->SetViewport(&viewport);
        }
        else
        {
          device_->SetViewport(&back_viewport_);
        }

        device_->SetDepthStencilSurface(depth_buf[3]);
        device_->SetTransform(D3DTS_WORLD, &dworld);
        device_->SetTransform(D3DTS_VIEW, &dview);
      };

    for ( int i = 0, len = ExpGetAcsNum(); i < len; i++ )
    {
      if ( ExpGetAcsOrder(i) == ExpGetCurrentObject() )
      {
        Draw(ExpGetAcsWorldMat(i));
        break;
      }
    }
    for ( int i = 0, len = ExpGetPmdNum(); i < len; i++ )
    {
      if ( ExpGetPmdOrder(i) == ExpGetCurrentObject() )
      {
        Draw(ExpGetPmdBoneWorldMat(i, 0));
        break;
      }
    }
    now_draw = 0;
  }

  void PostDrawIndexedPrimitive(D3DPRIMITIVETYPE, INT /*BaseVertexIndex*/, UINT /*MinVertexIndex*/, UINT /*NumVertices*/, UINT /*startIndex*/, UINT /*primCount*/, HRESULT& /*res*/) override
  {
    device_->SetViewport(&back_viewport_);
    if ( tmp_depth ) device_->SetDepthStencilSurface(tmp_depth);

    return;
  }

  void Clear(DWORD Count, const D3DRECT* pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil) override
  {
    static int now_clear = 0;
    if ( now_clear ) return;
    now_clear = 1;
    IDirect3DSurface9* tmp;
    device_->GetDepthStencilSurface(&tmp);
    for ( auto& i : depth_buf )
    {
      device_->SetDepthStencilSurface(i);
      device_->Clear(Count, pRects, Flags, Color, Z, Stencil);
    }

    device_->SetDepthStencilSurface(tmp);
    if ( tmp ) tmp->Release();
    now_clear = 0;
  }


  const char* getPluginTitle() const override { return "MMDUtility_ViewSplit"; }

  IDirect3DSurface9* depth_buf[4] = {};
  D3DVIEWPORT9 back_viewport_;
  IDirect3DSurface9* tmp_depth = nullptr;
  IDirect3DDevice9* device_;
};

int version() { return 3; }

MMDPluginDLL3* create3(IDirect3DDevice9* device)
{
  return new ViewSplit(device);
}

void destroy3(MMDPluginDLL3* p)
{
  delete p;
}
