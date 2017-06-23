// Copyright (C) 2017 Elviss Strazdins
// This file is part of the Ouzel engine.

#include "core/CompileConfig.h"

#if OUZEL_SUPPORTS_DIRECT3D11

#include "TextureResourceD3D11.h"
#include "RendererD3D11.h"
#include "core/Engine.h"
#include "utils/Log.h"

namespace ouzel
{
    namespace graphics
    {
        static DXGI_FORMAT getD3D11PixelFormat(PixelFormat pixelFormat)
        {
            switch (pixelFormat)
            {
                case PixelFormat::A8_UNORM: return DXGI_FORMAT_A8_UNORM;
                case PixelFormat::R8_UNORM: return DXGI_FORMAT_R8_UNORM;
                case PixelFormat::R8_SNORM: return DXGI_FORMAT_R8_SNORM;
                case PixelFormat::R8_UINT: return DXGI_FORMAT_R8_UINT;
                case PixelFormat::R8_SINT: return DXGI_FORMAT_R8_SINT;
                case PixelFormat::R16_UNORM: return DXGI_FORMAT_R16_UNORM;
                case PixelFormat::R16_SNORM: return DXGI_FORMAT_R16_SNORM;
                case PixelFormat::R16_UINT: return DXGI_FORMAT_R16_UINT;
                case PixelFormat::R16_SINT: return DXGI_FORMAT_R16_SINT;
                case PixelFormat::R16_FLOAT: return DXGI_FORMAT_R16_FLOAT;
                case PixelFormat::R32_UINT: return DXGI_FORMAT_R32_UINT;
                case PixelFormat::R32_SINT: return DXGI_FORMAT_R32_SINT;
                case PixelFormat::R32_FLOAT: return DXGI_FORMAT_R32_FLOAT;
                case PixelFormat::RG8_UNORM: return DXGI_FORMAT_R8G8_UNORM;
                case PixelFormat::RG8_SNORM: return DXGI_FORMAT_R8G8_SNORM;
                case PixelFormat::RG8_UINT: return DXGI_FORMAT_R8G8_UINT;
                case PixelFormat::RG8_SINT: return DXGI_FORMAT_R8G8_SINT;
                case PixelFormat::RGBA8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM;
                case PixelFormat::RGBA8_SNORM: return DXGI_FORMAT_R8G8B8A8_SNORM;
                case PixelFormat::RGBA8_UINT: return DXGI_FORMAT_R8G8B8A8_UINT;
                case PixelFormat::RGBA8_SINT: return DXGI_FORMAT_R8G8B8A8_SINT;
                case PixelFormat::RGBA16_UNORM: return DXGI_FORMAT_R16G16B16A16_UNORM;
                case PixelFormat::RGBA16_SNORM: return DXGI_FORMAT_R16G16B16A16_SNORM;
                case PixelFormat::RGBA16_UINT: return DXGI_FORMAT_R16G16B16A16_UINT;
                case PixelFormat::RGBA16_SINT: return DXGI_FORMAT_R16G16B16A16_SINT;
                case PixelFormat::RGBA16_FLOAT: return DXGI_FORMAT_R16G16B16A16_FLOAT;
                case PixelFormat::RGBA32_UINT: return DXGI_FORMAT_R32G32B32A32_UINT;
                case PixelFormat::RGBA32_SINT: return DXGI_FORMAT_R32G32B32A32_SINT;
                case PixelFormat::RGBA32_FLOAT: return DXGI_FORMAT_R32G32B32A32_FLOAT;
                default: return DXGI_FORMAT_UNKNOWN;
            }
        }

        TextureResourceD3D11::TextureResourceD3D11()
        {
        }

        TextureResourceD3D11::~TextureResourceD3D11()
        {
            if (depthStencilTexture)
            {
                depthStencilTexture->Release();
            }

            if (depthStencilView)
            {
                depthStencilView->Release();
            }

            if (renderTargetView)
            {
                renderTargetView->Release();
            }

            if (resourceView)
            {
                resourceView->Release();
            }

            if (texture)
            {
                texture->Release();
            }

            if (samplerState)
            {
                samplerState->Release();
            }

            width = 0;
            height = 0;
        }

        bool TextureResourceD3D11::upload()
        {
            std::lock_guard<std::mutex> lock(uploadMutex);

            if (dirty)
            {
                RendererD3D11* rendererD3D11 = static_cast<RendererD3D11*>(sharedEngine->getRenderer());

                if ((dirty & DIRTY_DATA) ||
                    (dirty & DIRTY_SIZE))
                {
                    if (size.v[0] > 0 &&
                        size.v[1] > 0)
                    {
                        if (!texture || (dirty & DIRTY_SIZE))
                        {
                            if (texture)
                            {
                                texture->Release();
                            }

                            if (resourceView)
                            {
                                resourceView->Release();
                                resourceView = nullptr;
                            }

                            if (renderTargetView)
                            {
                                renderTargetView->Release();
                                renderTargetView = nullptr;
                            }

                            if (depthStencilTexture)
                            {
                                depthStencilTexture->Release();
                                depthStencilTexture = nullptr;
                            }

                            if (depthStencilView)
                            {
                                depthStencilView->Release();
                                depthStencilView = nullptr;
                            }

                            width = static_cast<UINT>(size.v[0]);
                            height = static_cast<UINT>(size.v[1]);

                            DXGI_FORMAT d3d11PixelFormat = getD3D11PixelFormat(pixelFormat);

                            if (d3d11PixelFormat == DXGI_FORMAT_UNKNOWN)
                            {
                                Log(Log::Level::ERR) << "Invalid pixel format";
                                return false;
                            }

                            D3D11_TEXTURE2D_DESC textureDesc;
                            textureDesc.Width = width;
                            textureDesc.Height = height;
                            textureDesc.MipLevels = (levels.size() > 1) ? 0 : 1;
                            textureDesc.ArraySize = 1;
                            textureDesc.Format = d3d11PixelFormat;
                            textureDesc.SampleDesc.Count = sampleCount;
                            textureDesc.SampleDesc.Quality = 0;
                            if (renderTarget) textureDesc.Usage = D3D11_USAGE_DEFAULT;
                            else if (dynamic) textureDesc.Usage = D3D11_USAGE_DYNAMIC;
                            else textureDesc.Usage = D3D11_USAGE_IMMUTABLE;
                            textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | (renderTarget ? D3D11_BIND_RENDER_TARGET : 0);
                            textureDesc.CPUAccessFlags = (dynamic && !renderTarget) ? D3D11_CPU_ACCESS_WRITE : 0;
                            textureDesc.MiscFlags = 0;

                            if (levels.empty() || renderTarget)
                            {
                                HRESULT hr = rendererD3D11->getDevice()->CreateTexture2D(&textureDesc, nullptr, &texture);
                                if (FAILED(hr))
                                {
                                    Log(Log::Level::ERR) << "Failed to create Direct3D 11 texture, error: " << hr;
                                    return false;
                                }
                            }
                            else
                            {
                                std::vector<D3D11_SUBRESOURCE_DATA> subresourceData(levels.size());

                                for (size_t level = 0; level < levels.size(); ++level)
                                {
                                    subresourceData[level].pSysMem = levels[level].data.data();
                                    subresourceData[level].SysMemPitch = static_cast<UINT>(levels[level].pitch);
                                    subresourceData[level].SysMemSlicePitch = 0;
                                }

                                HRESULT hr = rendererD3D11->getDevice()->CreateTexture2D(&textureDesc, subresourceData.data(), &texture);
                                if (FAILED(hr))
                                {
                                    Log(Log::Level::ERR) << "Failed to create Direct3D 11 texture, error: " << hr;
                                    return false;
                                }
                            }

                            D3D11_SHADER_RESOURCE_VIEW_DESC resourceViewDesc;
                            resourceViewDesc.Format = d3d11PixelFormat;
                            resourceViewDesc.ViewDimension = (sampleCount > 1) ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;

                            if (sampleCount == 1)
                            {
                                resourceViewDesc.Texture2D.MostDetailedMip = 0;
                                resourceViewDesc.Texture2D.MipLevels = static_cast<UINT>(levels.size());
                            }

                            HRESULT hr = rendererD3D11->getDevice()->CreateShaderResourceView(texture, &resourceViewDesc, &resourceView);
                            if (FAILED(hr))
                            {
                                Log(Log::Level::ERR) << "Failed to create Direct3D 11 shader resource view, error: " << hr;
                                return false;
                            }

                            if (renderTarget)
                            {
                                D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
                                renderTargetViewDesc.Format = d3d11PixelFormat;
                                renderTargetViewDesc.ViewDimension = (sampleCount > 1) ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;

                                if (sampleCount == 1)
                                {
                                    renderTargetViewDesc.Texture2D.MipSlice = 0;
                                }

                                hr = rendererD3D11->getDevice()->CreateRenderTargetView(texture, &renderTargetViewDesc, &renderTargetView);
                                if (FAILED(hr))
                                {
                                    Log(Log::Level::ERR) << "Failed to create Direct3D 11 render target view, error: " << hr;
                                    return false;
                                }
                            }

                            if (depth)
                            {
                                D3D11_TEXTURE2D_DESC depthStencilDesc;
                                depthStencilDesc.Width = width;
                                depthStencilDesc.Height = height;
                                depthStencilDesc.MipLevels = 1;
                                depthStencilDesc.ArraySize = 1;
                                depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
                                depthStencilDesc.SampleDesc.Count = sampleCount;
                                depthStencilDesc.SampleDesc.Quality = 0;
                                depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
                                depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
                                depthStencilDesc.CPUAccessFlags = 0;
                                depthStencilDesc.MiscFlags = 0;
                                hr = rendererD3D11->getDevice()->CreateTexture2D(&depthStencilDesc, nullptr, &depthStencilTexture);
                                if (FAILED(hr))
                                {
                                    Log(Log::Level::ERR) << "Failed to create Direct3D 11 depth stencil texture, error: " << hr;
                                    return false;
                                }

                                hr = rendererD3D11->getDevice()->CreateDepthStencilView(depthStencilTexture, nullptr, &depthStencilView);
                                if (FAILED(hr))
                                {
                                    Log(Log::Level::ERR) << "Failed to create Direct3D 11 depth stencil view, error: " << hr;
                                    return false;
                                }
                            }
                        }
                        else if (!renderTarget)
                        {
                            if (dynamic)
                            {
                                for (size_t level = 0; level < levels.size(); ++level)
                                {
                                    if (!levels[level].data.empty())
                                    {
                                        D3D11_MAPPED_SUBRESOURCE mappedSubresource;
                                        mappedSubresource.pData = nullptr;
                                        mappedSubresource.RowPitch = 0;
                                        mappedSubresource.DepthPitch = 0;
                                    
                                        HRESULT hr = rendererD3D11->getContext()->Map(texture, static_cast<UINT>(level),
                                                                                      (level == 0) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE,
                                                                                      0, &mappedSubresource);

                                        if (FAILED(hr))
                                        {
                                            Log(Log::Level::ERR) << "Failed to map Direct3D 11 texture, error: " << hr;
                                            return false;
                                        }

                                        uint8_t* destination = static_cast<uint8_t*>(mappedSubresource.pData);
                                    
                                        if (mappedSubresource.RowPitch == levels[level].pitch)
                                        {
                                            std::copy(levels[level].data.begin(),
                                                      levels[level].data.end(),
                                                      destination);
                                        }
                                        else
                                        {
                                            auto source = levels[level].data.begin();
                                            UINT rowSize = static_cast<UINT>(levels[level].size.v[0]) * getPixelSize(pixelFormat);
                                            UINT rows = static_cast<UINT>(levels[level].size.v[1]);

                                            for (UINT row = 0; row < rows; ++row)
                                            {
                                                std::copy(source,
                                                          source + rowSize,
                                                          destination);

                                                source += levels[level].pitch;
                                                destination += mappedSubresource.RowPitch;
                                            }
                                        }

                                        rendererD3D11->getContext()->Unmap(texture, static_cast<UINT>(level));
                                    }
                                }
                            }
                            else
                            {
                                for (size_t level = 0; level < levels.size(); ++level)
                                {
                                    if (!levels[level].data.empty())
                                    {
                                        rendererD3D11->getContext()->UpdateSubresource(texture, static_cast<UINT>(level),
                                                                                       nullptr, levels[level].data.data(),
                                                                                       static_cast<UINT>(levels[level].pitch), 0);
                                    }
                                }
                            }
                        }
                    }
                }

                if (dirty & DIRTY_PARAMETERS)
                {
                    clearFrameBufferView = clearColorBuffer;
                    clearDepthBufferView = clearDepthBuffer;

                    frameBufferClearColor[0] = clearColor.normR();
                    frameBufferClearColor[1] = clearColor.normG();
                    frameBufferClearColor[2] = clearColor.normB();
                    frameBufferClearColor[3] = clearColor.normA();

                    if (samplerState) samplerState->Release();

                    RendererD3D11::SamplerStateDesc samplerDesc;
                    samplerDesc.filter = (filter == Texture::Filter::DEFAULT) ? rendererD3D11->getTextureFilter() : filter;
                    samplerDesc.addressX = addressX;
                    samplerDesc.addressY = addressY;
                    samplerDesc.maxAnisotropy = (maxAnisotropy == 0) ? rendererD3D11->getMaxAnisotropy() : maxAnisotropy;

                    samplerState = rendererD3D11->getSamplerState(samplerDesc);

                    if (!samplerState)
                    {
                        Log(Log::Level::ERR) << "Failed to get D3D11 sampler state";
                        return false;
                    }

                    samplerState->AddRef();
                }

                dirty = 0;
            }

            return true;
        }
    } // namespace graphics
} // namespace ouzel

#endif
