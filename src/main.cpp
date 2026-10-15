#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dstorage.h>
#include <wrl/client.h>
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <regex>
#include <algorithm>
#include <iomanip>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>

namespace fs = std::filesystem;
using Microsoft::WRL::ComPtr;

#pragma pack(push, 1)
struct DDS_PIXELFORMAT_RAW {
	uint32_t size;
	uint32_t flags;
	uint32_t fourCC;
	uint32_t rgbBitCount;
	uint32_t rBitMask;
	uint32_t gBitMask;
	uint32_t bBitMask;
	uint32_t aBitMask;
};

struct DDS_HEADER_RAW {
	uint32_t size;
	uint32_t flags;
	uint32_t height;
	uint32_t width;
	uint32_t pitchOrLinearSize;
	uint32_t depth;
	uint32_t mipMapCount;
	uint32_t reserved1[11];
	DDS_PIXELFORMAT_RAW ddspf;
	uint32_t caps;
	uint32_t caps2;
	uint32_t caps3;
	uint32_t caps4;
	uint32_t reserved2; // Offset 120 in header (124 in DDS file)
};

struct DDS_HEADER_DXT10_RAW {
	uint32_t dxgiFormat;
	uint32_t resourceDimension;
	uint32_t miscFlag;
	uint32_t arraySize;
	uint32_t miscFlags2;
};

struct GDDS_CHUNK_DESC {
	uint32_t fileOffset;
	uint32_t pad;
	uint32_t compressedSize;
	uint32_t uncompressedSize;
};
#pragma pack(pop)

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3) \
	((uint32_t)(uint8_t)(ch0) | ((uint32_t)(uint8_t)(ch1) << 8) | \
	((uint32_t)(uint8_t)(ch2) << 16) | ((uint32_t)(uint8_t)(ch3) << 24))
#endif

static bool GetTextureFormatInfo(
	const DDS_HEADER_RAW& header,
	const DDS_HEADER_DXT10_RAW* dxt10,
	uint32_t& outDxgiFormat,
	uint32_t& outBlockBytes,
	bool& outIsBlockCompressed)
{
	outIsBlockCompressed = true;
	if (dxt10) {
		outDxgiFormat = dxt10->dxgiFormat;
		switch (dxt10->dxgiFormat) {
		case 70: // BC1_TYPELESS
		case 71: // BC1_UNORM
		case 72: // BC1_UNORM_SRGB
		case 79: // BC4_TYPELESS
		case 80: // BC4_UNORM
		case 81: // BC4_SNORM
			outBlockBytes = 8;
			return true;
		case 73: // BC2_TYPELESS
		case 74: // BC2_UNORM
		case 75: // BC2_UNORM_SRGB
		case 76: // BC3_TYPELESS
		case 77: // BC3_UNORM
		case 78: // BC3_UNORM_SRGB
		case 82: // BC5_TYPELESS
		case 83: // BC5_UNORM
		case 84: // BC5_SNORM
		case 94: // BC6H_TYPELESS
		case 95: // BC6H_UF16
		case 96: // BC6H_SF16
		case 97: // BC7_TYPELESS
		case 98: // BC7_UNORM
		case 99: // BC7_UNORM_SRGB
			outBlockBytes = 16;
			return true;
		case 27: // R8G8B8A8_TYPELESS
		case 28: // R8G8B8A8_UNORM
		case 29: // R8G8B8A8_UNORM_SRGB
		case 30: // R8G8B8A8_UINT
		case 31: // R8G8B8A8_SNORM
		case 32: // R8G8B8A8_SINT
		case 87: // B8G8R8A8_UNORM
		case 90: // B8G8R8A8_TYPELESS
		case 91: // B8G8R8A8_UNORM_SRGB
		case 92: // B8G8R8X8_TYPELESS
		case 93: // B8G8R8X8_UNORM_SRGB
			outBlockBytes = 4;
			outIsBlockCompressed = false;
			return true;
		default:
			return false;
		}
	}

	if (header.ddspf.flags & 0x00000004) {
		switch (header.ddspf.fourCC) {
		case MAKEFOURCC('D', 'X', 'T', '1'):
			outDxgiFormat = 71; // BC1_UNORM
			outBlockBytes = 8;
			return true;
		case MAKEFOURCC('D', 'X', 'T', '3'):
			outDxgiFormat = 74; // BC2_UNORM
			outBlockBytes = 16;
			return true;
		case MAKEFOURCC('D', 'X', 'T', '5'):
			outDxgiFormat = 77; // BC3_UNORM
			outBlockBytes = 16;
			return true;
		case MAKEFOURCC('A', 'T', 'I', '1'):
		case MAKEFOURCC('B', 'C', '4', 'U'):
			outDxgiFormat = 80; // BC4_UNORM
			outBlockBytes = 8;
			return true;
		case MAKEFOURCC('A', 'T', 'I', '2'):
		case MAKEFOURCC('B', 'C', '5', 'U'):
			outDxgiFormat = 83; // BC5_UNORM
			outBlockBytes = 16;
			return true;
		case MAKEFOURCC('B', 'C', '7', '\0'):
		case MAKEFOURCC('B', 'C', '7', ' '):
			outDxgiFormat = 98; // BC7_UNORM
			outBlockBytes = 16;
			return true;
		case 113: // D3DFMT_A16B16G16R16F (fourCC 0x71)
			outDxgiFormat = 10; // R16G16B16A16_FLOAT
			outBlockBytes = 8;
			outIsBlockCompressed = false;
			return true;
		}
	}

	// Legacy uncompressed DDS formats (RGB, RGBA)
	if (header.ddspf.flags & 0x00000040) { // DDPF_RGB
		if (header.ddspf.rgbBitCount == 32) {
			if (header.ddspf.rBitMask == 0x000000ff && header.ddspf.gBitMask == 0x0000ff00 &&
				header.ddspf.bBitMask == 0x00ff0000) {
				outDxgiFormat = 28; // R8G8B8A8_UNORM
				outBlockBytes = 4;
				outIsBlockCompressed = false;
				return true;
			}
			if (header.ddspf.rBitMask == 0x00ff0000 && header.ddspf.gBitMask == 0x0000ff00 &&
				header.ddspf.bBitMask == 0x000000ff) {
				outDxgiFormat = 87; // B8G8R8A8_UNORM
				outBlockBytes = 4;
				outIsBlockCompressed = false;
				return true;
			}
		}
	}

	return false;
}

static std::string WideToUtf8(const std::wstring& w)
{
	if (w.empty()) return {};
	int len = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
	if (len <= 0) return {};
	std::string s(len, '\0');
	WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), &s[0], len, nullptr, nullptr);
	return s;
}

static std::string ToLowerAscii(std::string s)
{
	for (char& c : s) {
		if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
	}
	return s;
}

static std::wstring NormalizePath(const fs::path& p)
{
	std::wstring s = p.wstring();
	if (s.rfind(L"\\\\?\\", 0) == 0) return s;
	if (s.size() >= 240) {
		std::replace(s.begin(), s.end(), L'/', L'\\');
		return L"\\\\?\\" + s;
	}
	return s;
}

static bool ReadAllBytes(const fs::path& p, std::vector<uint8_t>& out)
{
	std::wstring norm = NormalizePath(p);
	HANDLE h = CreateFileW(norm.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE) return false;
	LARGE_INTEGER size;
	if (!GetFileSizeEx(h, &size) || size.QuadPart == 0) {
		CloseHandle(h);
		return false;
	}
	out.resize(static_cast<size_t>(size.QuadPart));
	DWORD readBytes = 0;
	BOOL ok = ReadFile(h, out.data(), static_cast<DWORD>(size.QuadPart), &readBytes, nullptr);
	CloseHandle(h);
	return ok && (readBytes == size.QuadPart);
}

static bool WriteAllBytes(const fs::path& p, const void* data, size_t size)
{
	std::wstring norm = NormalizePath(p);
	HANDLE h = CreateFileW(norm.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE) return false;
	DWORD written = 0;
	BOOL ok = WriteFile(h, data, static_cast<DWORD>(size), &written, nullptr);
	CloseHandle(h);
	return ok && (written == size);
}

static bool PathExists(const fs::path& p)
{
	std::wstring norm = NormalizePath(p);
	DWORD attr = GetFileAttributesW(norm.c_str());
	return (attr != INVALID_FILE_ATTRIBUTES);
}

static bool IsDirectory(const fs::path& p)
{
	std::wstring norm = NormalizePath(p);
	DWORD attr = GetFileAttributesW(norm.c_str());
	return (attr != INVALID_FILE_ATTRIBUTES) && ((attr & FILE_ATTRIBUTE_DIRECTORY) != 0);
}

static void FindFilesRecursiveWin32(const std::wstring& dir, const std::wstring& extFilter, std::vector<fs::path>& outFiles)
{
	std::wstring searchPath = NormalizePath(dir) + L"\\*";
	WIN32_FIND_DATAW fd;
	HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);
	if (hFind == INVALID_HANDLE_VALUE) return;

	do {
		if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;

		std::wstring fullPath = dir + L"\\" + fd.cFileName;
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			FindFilesRecursiveWin32(fullPath, extFilter, outFiles);
		} else {
			size_t dot = fullPath.rfind(L'.');
			if (dot != std::wstring::npos) {
				std::wstring ext = fullPath.substr(dot);
				for (auto& c : ext) c = towlower(c);
				if (ext == extFilter) {
					outFiles.push_back(fs::path(fullPath));
				}
			}
		}
	} while (FindNextFileW(hFind, &fd));

	FindClose(hFind);
}

enum class MipMode {
	Strip, // Mip 0 only (maximum compression, default)
	Keep   // Preserve full mipmap pyramid
};

static uint64_t CalculateTextureTotalSize(
	uint32_t width,
	uint32_t height,
	uint32_t mipCount,
	uint32_t blockOrPixelBytes,
	bool isBlockCompressed)
{
	uint64_t total = 0;
	uint32_t w = width;
	uint32_t h = height;
	uint32_t mips = std::max(1u, mipCount);

	for (uint32_t i = 0; i < mips; ++i) {
		uint32_t curW = std::max(1u, w);
		uint32_t curH = std::max(1u, h);
		uint64_t mipBytes = 0;
		if (isBlockCompressed) {
			uint32_t blocksX = std::max(1u, (curW + 3) / 4);
			uint32_t blocksY = std::max(1u, (curH + 3) / 4);
			mipBytes = static_cast<uint64_t>(blocksX) * blocksY * blockOrPixelBytes;
		} else {
			mipBytes = static_cast<uint64_t>(curW) * curH * blockOrPixelBytes;
		}
		total += mipBytes;
		if (w > 1) w >>= 1;
		if (h > 1) h >>= 1;
	}
	return total;
}

static bool CompressDdsToGdds(
	IDStorageCompressionCodec* codec,
	const fs::path& inputPath,
	const fs::path& outputPath,
	uint64_t& outOrigSize,
	uint64_t& outGddsSize,
	std::wstring& outSkipReason,
	MipMode mipMode = MipMode::Strip)
{
	std::vector<uint8_t> fileBytes;
	if (!ReadAllBytes(inputPath, fileBytes)) {
		outSkipReason = L"Cannot open file";
		return false;
	}

	if (fileBytes.size() < 128) {
		outSkipReason = L"File too small for DDS";
		return false;
	}

	if (memcmp(fileBytes.data(), "DDS ", 4) != 0) {
		outSkipReason = L"Not a DDS file (bad magic)";
		return false;
	}

	const DDS_HEADER_RAW* ddsHeader = reinterpret_cast<const DDS_HEADER_RAW*>(fileBytes.data() + 4);
	if (ddsHeader->size != sizeof(DDS_HEADER_RAW)) {
		outSkipReason = L"Invalid DDS header size";
		return false;
	}

	const DDS_HEADER_DXT10_RAW* dxt10Header = nullptr;
	size_t rawDataOffset = 128;

	if ((ddsHeader->ddspf.flags & 0x00000004) && ddsHeader->ddspf.fourCC == MAKEFOURCC('D', 'X', '1', '0')) {
		if (fileBytes.size() < 148) {
			outSkipReason = L"Incomplete DX10 header";
			return false;
		}
		dxt10Header = reinterpret_cast<const DDS_HEADER_DXT10_RAW*>(fileBytes.data() + 128);
		rawDataOffset = 148;
	}

	uint32_t dxgiFormat = 0;
	uint32_t blockOrPixelBytes = 0;
	bool isBlockCompressed = true;
	if (!GetTextureFormatInfo(*ddsHeader, dxt10Header, dxgiFormat, blockOrPixelBytes, isBlockCompressed)) {
		outSkipReason = L"Unsupported format";
		return false;
	}

	uint32_t width = ddsHeader->width;
	uint32_t height = ddsHeader->height;
	uint32_t srcMipCount = std::max(1u, ddsHeader->mipMapCount);

	uint32_t rowPitch = 0;
	uint64_t mip0DataSize = 0;
	if (isBlockCompressed) {
		uint32_t blocksX = std::max(1u, (width + 3) / 4);
		uint32_t blocksY = std::max(1u, (height + 3) / 4);
		rowPitch = blocksX * blockOrPixelBytes;
		mip0DataSize = static_cast<uint64_t>(rowPitch) * blocksY;
	} else {
		rowPitch = width * blockOrPixelBytes;
		mip0DataSize = static_cast<uint64_t>(rowPitch) * height;
	}

	uint32_t targetMipCount = (mipMode == MipMode::Keep && srcMipCount > 1) ? srcMipCount : 1;
	uint64_t requiredDataSize = (targetMipCount > 1)
		? CalculateTextureTotalSize(width, height, targetMipCount, blockOrPixelBytes, isBlockCompressed)
		: mip0DataSize;

	if (fileBytes.size() < rawDataOffset + requiredDataSize) {
		outSkipReason = L"File truncated";
		return false;
	}

	const uint8_t* rawData = fileBytes.data() + rawDataOffset;

	// When retaining sub-mips, compress all mip levels into a single chunk.
	// For Mip 0 only, slice large tall textures (height > 2048) into chunks of 2048 px for DirectStorage streaming.
	uint32_t chunkCount = 1;
	if (targetMipCount == 1 && height > 2048 && (height % 2048 == 0)) {
		chunkCount = height / 2048;
		if (chunkCount > 16) chunkCount = 16;
	}

	uint32_t sliceHeight = height / chunkCount;
	uint32_t sliceBlocksY = isBlockCompressed ? std::max(1u, (sliceHeight + 3) / 4) : sliceHeight;
	uint32_t sliceUncompSize = (chunkCount > 1) ? (rowPitch * sliceBlocksY) : static_cast<uint32_t>(requiredDataSize);

	struct CompressedChunk {
		std::vector<uint8_t> data;
		uint32_t uncompSize = 0;
	};

	std::vector<CompressedChunk> chunks(chunkCount);
	size_t maxBound = codec->CompressBufferBound(sliceUncompSize);
	std::vector<uint8_t> compBuffer(maxBound);
	std::vector<uint8_t> verifyBuffer(sliceUncompSize);

	for (uint32_t i = 0; i < chunkCount; ++i) {
		const uint8_t* sliceData = rawData + (static_cast<uint64_t>(i) * sliceUncompSize);
		size_t compSize = 0;

		HRESULT hr = codec->CompressBuffer(
			sliceData,
			sliceUncompSize,
			DSTORAGE_COMPRESSION_BEST_RATIO,
			compBuffer.data(),
			compBuffer.size(),
			&compSize);

		if (FAILED(hr) || compSize == 0) {
			outSkipReason = L"GDeflate compression failed";
			return false;
		}

		size_t decompSize = 0;
		hr = codec->DecompressBuffer(
			compBuffer.data(),
			compSize,
			verifyBuffer.data(),
			verifyBuffer.size(),
			&decompSize);

		if (FAILED(hr) || decompSize != sliceUncompSize || memcmp(verifyBuffer.data(), sliceData, sliceUncompSize) != 0) {
			outSkipReason = L"Lossless verification failed";
			return false;
		}

		chunks[i].data.assign(compBuffer.data(), compBuffer.data() + compSize);
		chunks[i].uncompSize = sliceUncompSize;
	}

	uint32_t headerTotalSize = 148 + (chunkCount * sizeof(GDDS_CHUNK_DESC));
	std::vector<uint8_t> outBytes(headerTotalSize);

	memcpy(outBytes.data(), "GDDS", 4);

	DDS_HEADER_RAW outDds = *ddsHeader;
	outDds.mipMapCount = targetMipCount;
	outDds.reserved2 = chunkCount;
	outDds.ddspf.size = sizeof(DDS_PIXELFORMAT_RAW);
	outDds.ddspf.flags = 0x00000004; // DDPF_FOURCC
	outDds.ddspf.fourCC = MAKEFOURCC('D', 'X', '1', '0');
	memcpy(outBytes.data() + 4, &outDds, sizeof(DDS_HEADER_RAW));

	DDS_HEADER_DXT10_RAW outDx10{};
	if (dxt10Header) {
		outDx10 = *dxt10Header;
	} else {
		outDx10.dxgiFormat = dxgiFormat;
		outDx10.resourceDimension = 3;
		outDx10.miscFlag = 0;
		outDx10.arraySize = 1;
		outDx10.miscFlags2 = 1;
	}
	memcpy(outBytes.data() + 128, &outDx10, sizeof(DDS_HEADER_DXT10_RAW));

	uint32_t currentFileOffset = headerTotalSize;
	for (uint32_t i = 0; i < chunkCount; ++i) {
		GDDS_CHUNK_DESC desc{};
		desc.fileOffset = currentFileOffset;
		desc.pad = 0;
		desc.compressedSize = static_cast<uint32_t>(chunks[i].data.size());
		desc.uncompressedSize = chunks[i].uncompSize;

		memcpy(outBytes.data() + 148 + (i * sizeof(GDDS_CHUNK_DESC)), &desc, sizeof(GDDS_CHUNK_DESC));
		currentFileOffset += desc.compressedSize;
	}

	for (uint32_t i = 0; i < chunkCount; ++i) {
		outBytes.insert(outBytes.end(), chunks[i].data.begin(), chunks[i].data.end());
	}

	fs::path tmpPath = outputPath;
	tmpPath += L".tmp";

	if (!WriteAllBytes(tmpPath, outBytes.data(), outBytes.size())) {
		outSkipReason = L"Cannot create temporary output";
		return false;
	}

	std::wstring normTmp = NormalizePath(tmpPath);
	std::wstring normOut = NormalizePath(outputPath);
	if (!MoveFileExW(normTmp.c_str(), normOut.c_str(), MOVEFILE_REPLACE_EXISTING)) {
		DeleteFileW(normTmp.c_str());
		outSkipReason = L"Rename temporary output failed";
		return false;
	}

	outOrigSize = fileBytes.size();
	outGddsSize = outBytes.size();
	return true;
}

static bool ReconcileIniReferences(const fs::path& iniPath)
{
	std::vector<uint8_t> inBytes;
	if (!ReadAllBytes(iniPath, inBytes)) return false;

	std::string content(reinterpret_cast<const char*>(inBytes.data()), inBytes.size());

	fs::path iniDir = iniPath.parent_path();
	bool modified = false;

	std::string newContent;
	newContent.reserve(content.size() + 256);

	size_t lastPos = 0;
	std::regex re(R"((filename\s*=\s*)([^\r\n]+?\.(?:dds|DDS|gdds|GDDS)))");
	auto words_begin = std::sregex_iterator(content.begin(), content.end(), re);
	auto words_end = std::sregex_iterator();

	for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
		std::smatch match = *i;
		newContent.append(content, lastPos, match.position() - lastPos);

		std::string prefix = match[1].str();
		std::string refPathStr = match[2].str();

		while (!refPathStr.empty() && (refPathStr.back() == ' ' || refPathStr.back() == '\t' || refPathStr.back() == '\r')) {
			refPathStr.pop_back();
		}

		fs::path refPath = fs::u8path(refPathStr);
		fs::path gddsRel = refPath;
		gddsRel.replace_extension(L".gdds");
		fs::path gddsFull = iniDir / gddsRel;

		fs::path ddsRel = refPath;
		ddsRel.replace_extension(L".dds");
		fs::path ddsFull = iniDir / ddsRel;

		if (PathExists(gddsFull)) {
			std::string gddsStr = WideToUtf8(gddsRel.wstring());
			newContent.append(prefix + gddsStr);
			if (refPath != gddsRel) modified = true;
		} else if (PathExists(ddsFull)) {
			std::string ddsStr = WideToUtf8(ddsRel.wstring());
			newContent.append(prefix + ddsStr);
			if (refPath != ddsRel) modified = true;
		} else {
			newContent.append(match[0].str());
		}

		lastPos = match.position() + match.length();
	}
	newContent.append(content, lastPos, content.size() - lastPos);

	if (modified) {
		std::wstring normIni = NormalizePath(iniPath);
		SetFileAttributesW(normIni.c_str(), FILE_ATTRIBUTE_NORMAL);
		return WriteAllBytes(iniPath, newContent.data(), newContent.size());
	}

	return false;
}

static bool DecompressGddsToDds(
	IDStorageCompressionCodec* codec,
	const fs::path& inPath,
	const fs::path& outPath,
	std::wstring& outSkipReason)
{
	std::vector<uint8_t> fileBytes;
	if (!ReadAllBytes(inPath, fileBytes)) {
		outSkipReason = L"Cannot open GDDS file";
		return false;
	}

	if (fileBytes.size() < 148 || memcmp(fileBytes.data(), "GDDS", 4) != 0) {
		outSkipReason = L"Not a GDDS file (bad magic or size)";
		return false;
	}

	const DDS_HEADER_RAW* ddsHeader = reinterpret_cast<const DDS_HEADER_RAW*>(fileBytes.data() + 4);
	const DDS_HEADER_DXT10_RAW* dxt10Header = reinterpret_cast<const DDS_HEADER_DXT10_RAW*>(fileBytes.data() + 128);
	uint32_t chunkCount = *(reinterpret_cast<const uint32_t*>(fileBytes.data() + 124));

	if (chunkCount == 0 || chunkCount > 64) {
		outSkipReason = L"Invalid chunk count";
		return false;
	}

	size_t chunkDescOffset = 148;
	if (fileBytes.size() < chunkDescOffset + chunkCount * sizeof(GDDS_CHUNK_DESC)) {
		outSkipReason = L"Incomplete chunk descriptors";
		return false;
	}

	const GDDS_CHUNK_DESC* chunks = reinterpret_cast<const GDDS_CHUNK_DESC*>(fileBytes.data() + chunkDescOffset);

	std::vector<uint8_t> allDecompData;
	for (uint32_t i = 0; i < chunkCount; ++i) {
		if (fileBytes.size() < chunks[i].fileOffset + chunks[i].compressedSize) {
			outSkipReason = L"Truncated chunk data";
			return false;
		}

		std::vector<uint8_t> chunkDecomp(chunks[i].uncompressedSize);
		size_t actualDecomp = 0;
		HRESULT hr = codec->DecompressBuffer(
			fileBytes.data() + chunks[i].fileOffset,
			chunks[i].compressedSize,
			chunkDecomp.data(),
			chunks[i].uncompressedSize,
			&actualDecomp);

		if (FAILED(hr) || actualDecomp != chunks[i].uncompressedSize) {
			outSkipReason = L"Chunk decompression failed";
			return false;
		}

		allDecompData.insert(allDecompData.end(), chunkDecomp.begin(), chunkDecomp.end());
	}

	DDS_HEADER_RAW outDdsHeader = *ddsHeader;
	outDdsHeader.reserved2 = 0;

	std::vector<uint8_t> outDds;
	outDds.reserve(148 + allDecompData.size());

	const char ddsMagic[4] = { 'D', 'D', 'S', ' ' };
	outDds.insert(outDds.end(), ddsMagic, ddsMagic + 4);
	const uint8_t* hdrBytes = reinterpret_cast<const uint8_t*>(&outDdsHeader);
	outDds.insert(outDds.end(), hdrBytes, hdrBytes + sizeof(DDS_HEADER_RAW));
	const uint8_t* dxt10Bytes = reinterpret_cast<const uint8_t*>(dxt10Header);
	outDds.insert(outDds.end(), dxt10Bytes, dxt10Bytes + sizeof(DDS_HEADER_DXT10_RAW));
	outDds.insert(outDds.end(), allDecompData.begin(), allDecompData.end());

	if (!WriteAllBytes(outPath, outDds.data(), outDds.size())) {
		outSkipReason = L"Failed to write output DDS";
		return false;
	}

	return true;
}

int wmain(int argc, wchar_t* argv[])
{
	std::wcout << L"=== High-Performance GDDS Converter for DirectStorage ===" << std::endl;

	if (argc < 2 || std::wstring(argv[1]) == L"--help" || std::wstring(argv[1]) == L"-h" || std::wstring(argv[1]) == L"/?") {
		std::wcout << L"Usage:\n"
			<< L"  Drag-and-Drop / Toggle Mode:\n"
			<< L"    gdds_tool.exe <file_or_folder> [options]\n"
			<< L"    - If .dds or folder of .dds:  Compresses to .gdds, replaces originals, updates .ini\n"
			<< L"    - If .gdds or folder of .gdds: Reverts back to .dds, replaces .gdds, updates .ini\n"
			<< L"\n"
			<< L"  Mipmap Options:\n"
			<< L"    --mips <strip|keep> Max compression strips sub-mips to Mip 0 (default: strip)\n"
			<< L"    --strip-mips        Strip sub-mips, compressing Mip 0 only (maximum compression)\n"
			<< L"    --keep-mips         Preserve all mipmap levels in the GDDS archive\n"
			<< L"    --preserve-mips     Alias for --keep-mips\n"
			<< L"\n"
			<< L"  Advanced Flags:\n"
			<< L"    --keep / -k         Keep original files (don't replace)\n"
			<< L"    --no-recursive      Don't search subdirectories\n"
			<< L"    --no-update-ini     Don't rewrite .ini references\n"
			<< L"    --threads N         Specify worker thread count\n"
			<< std::endl;
		return 0;
	}

	std::wstring firstArg = argv[1];
	fs::path firstPath(firstArg);

	// Parse flags across remaining arguments
	bool explicitDecompress = false;
	bool explicitCompress = false;
	bool keepOriginals = false;
	bool recursive = true;
	bool updateIni = true;
	MipMode mipMode = MipMode::Strip; // Default: maximum compression (Mip 0 only)
	unsigned int threadCount = std::min(32u, std::max(1u, std::thread::hardware_concurrency()));
	fs::path explicitOutPath;

	for (int i = 1; i < argc; ++i) {
		std::wstring arg = argv[i];
		if (arg == L"--decompress" || arg == L"-d" || arg == L"--decompress-dir") explicitDecompress = true;
		else if (arg == L"--dir" || arg == L"-c") explicitCompress = true;
		else if (arg == L"--keep" || arg == L"-k") keepOriginals = true;
		else if (arg == L"--no-recursive") recursive = false;
		else if (arg == L"--recursive" || arg == L"-r") recursive = true;
		else if (arg == L"--no-update-ini") updateIni = false;
		else if (arg == L"--update-ini") updateIni = true;
		else if (arg == L"--strip-mips") mipMode = MipMode::Strip;
		else if (arg == L"--keep-mips" || arg == L"--preserve-mips") mipMode = MipMode::Keep;
		else if (arg == L"--mips" && i + 1 < argc) {
			std::wstring val = argv[++i];
			for (auto& c : val) c = towlower(c);
			if (val == L"keep" || val == L"preserve" || val == L"all") {
				mipMode = MipMode::Keep;
			} else {
				mipMode = MipMode::Strip;
			}
		}
		else if (arg == L"--threads" && i + 1 < argc) {
			threadCount = static_cast<unsigned int>(_wtoi(argv[++i]));
			if (threadCount == 0) threadCount = 1;
		}
	}

	// Determine target path
	fs::path targetPath = firstPath;
	if (firstArg == L"--decompress" || firstArg == L"-d" || firstArg == L"--decompress-dir" || firstArg == L"--dir") {
		if (argc < 3) {
			std::wcerr << L"[ERROR] Missing path after " << firstArg << std::endl;
			return 1;
		}
		targetPath = argv[2];
		if (argc >= 4 && argv[3][0] != L'-') {
			explicitOutPath = argv[3];
		}
	}

	if (!PathExists(targetPath)) {
		std::wcerr << L"[ERROR] Target path does not exist: " << targetPath.wstring() << std::endl;
		return 1;
	}

	// -----------------------------------------------------------------------
	// Directory Mode (Auto-Toggle or Explicit)
	// -----------------------------------------------------------------------
	if (IsDirectory(targetPath)) {
		fs::path targetDir = targetPath;

		// Scan for both .dds and .gdds in the folder to auto-determine mode
		std::vector<fs::path> ddsFiles;
		std::vector<fs::path> gddsFiles;

		if (recursive) {
			FindFilesRecursiveWin32(targetDir.wstring(), L".dds", ddsFiles);
			FindFilesRecursiveWin32(targetDir.wstring(), L".gdds", gddsFiles);
		} else {
			std::wstring searchPath = NormalizePath(targetDir) + L"\\*";
			WIN32_FIND_DATAW fd;
			HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);
			if (hFind != INVALID_HANDLE_VALUE) {
				do {
					if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
						std::wstring fname = fd.cFileName;
						size_t dot = fname.rfind(L'.');
						if (dot != std::wstring::npos) {
							std::wstring ext = fname.substr(dot);
							for (auto& c : ext) c = towlower(c);
							if (ext == L".dds") ddsFiles.push_back(targetDir / fname);
							else if (ext == L".gdds") gddsFiles.push_back(targetDir / fname);
						}
					}
				} while (FindNextFileW(hFind, &fd));
				FindClose(hFind);
			}
		}

		bool decompressMode = false;
		if (explicitDecompress) {
			decompressMode = true;
		} else if (explicitCompress) {
			decompressMode = false;
		} else {
			// Auto-toggle: If there are .gdds files and few/no .dds files, or more .gdds than .dds -> Revert to .dds!
			if (ddsFiles.empty() && !gddsFiles.empty()) {
				decompressMode = true;
			} else if (!gddsFiles.empty() && gddsFiles.size() >= ddsFiles.size()) {
				decompressMode = true;
			} else {
				decompressMode = false;
			}
		}

		// --- Directory Decompression ---
		if (decompressMode) {
			std::wcout << L"Target Directory: " << targetDir.wstring() << std::endl;
			std::wcout << L"Action: Reverting GDDS -> DDS (" << gddsFiles.size() << L" files)" << std::endl;
			std::wcout << L"Replace Originals: " << (keepOriginals ? L"No (Keeping .gdds)" : L"Yes (Cleaning up .gdds)")
				<< L" | Update INI: " << (updateIni ? L"Yes" : L"No")
				<< L" | Worker Threads: " << threadCount << std::endl;

			if (gddsFiles.empty()) {
				std::wcout << L"No .gdds files found to decompress." << std::endl;
				return 0;
			}

			std::atomic<size_t> nextIndex{0};
			std::atomic<int> successCount{0};
			std::atomic<int> skippedCount{0};
			std::mutex printMutex;

			auto worker = [&]() {
				ComPtr<IDStorageCompressionCodec> codec;
				HRESULT hr = DStorageCreateCompressionCodec(DSTORAGE_COMPRESSION_FORMAT_GDEFLATE, 0, IID_PPV_ARGS(&codec));
				if (FAILED(hr) || !codec) return;

				size_t total = gddsFiles.size();
				while (true) {
					size_t idx = nextIndex.fetch_add(1);
					if (idx >= total) break;

					const fs::path& gddsPath = gddsFiles[idx];
					fs::path ddsPath = gddsPath;
					ddsPath.replace_extension(L".dds");

					std::wstring skipReason;
					if (DecompressGddsToDds(codec.Get(), gddsPath, ddsPath, skipReason)) {
						int sc = ++successCount;
						if (!keepOriginals) {
							DeleteFileW(NormalizePath(gddsPath).c_str());
						}
						if (sc % 100 == 0 || sc == static_cast<int>(total)) {
							std::lock_guard<std::mutex> lock(printMutex);
							std::wcout << L"Restored " << sc << L" / " << total << L" textures..." << std::endl;
						}
					} else {
						skippedCount++;
						std::lock_guard<std::mutex> lock(printMutex);
						std::wcerr << L"[FAIL] " << gddsPath.filename().wstring() << L": " << skipReason << std::endl;
					}
				}
			};

			std::vector<std::thread> workers;
			workers.reserve(threadCount);
			for (unsigned int t = 0; t < threadCount; ++t) {
				workers.emplace_back(worker);
			}
			for (auto& w : workers) {
				if (w.joinable()) w.join();
			}

			if (updateIni) {
				std::wcout << L"\nReconciling .ini references with local files..." << std::endl;
				std::vector<fs::path> iniFiles;
				if (recursive) {
					FindFilesRecursiveWin32(targetDir.wstring(), L".ini", iniFiles);
				} else {
					std::wstring searchPath = NormalizePath(targetDir) + L"\\*";
					WIN32_FIND_DATAW fd;
					HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);
					if (hFind != INVALID_HANDLE_VALUE) {
						do {
							if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
								std::wstring fname = fd.cFileName;
								size_t dot = fname.rfind(L'.');
								if (dot != std::wstring::npos) {
									std::wstring ext = fname.substr(dot);
									for (auto& c : ext) c = towlower(c);
									if (ext == L".ini") iniFiles.push_back(targetDir / fname);
								}
							}
						} while (FindNextFileW(hFind, &fd));
						FindClose(hFind);
					}
				}

				int inisUpdated = 0;
				for (const auto& iniPath : iniFiles) {
					if (ReconcileIniReferences(iniPath)) inisUpdated++;
				}
				std::wcout << L"[INI] Reconciled " << inisUpdated << L" configuration files." << std::endl;
			}

			std::wcout << L"\nDecompression complete: " << successCount.load() << L" textures restored to .dds." << std::endl;
			return 0;
		}

		// --- Directory Compression ---
		std::wcout << L"Target Directory: " << targetDir.wstring() << std::endl;
		std::wcout << L"Action: Compressing DDS -> GDDS (" << ddsFiles.size() << L" files)" << std::endl;
		std::wcout << L"Replace Originals: " << (keepOriginals ? L"No (Keeping .dds)" : L"Yes (Cleaning up .dds)")
			<< L" | Update INI: " << (updateIni ? L"Yes" : L"No")
			<< L" | Mipmaps: " << (mipMode == MipMode::Strip ? L"Strip to Mip 0 (Max Compression)" : L"Keep All Mips")
			<< L" | Worker Threads: " << threadCount << std::endl;

		if (ddsFiles.empty()) {
			std::wcout << L"No .dds files found to compress." << std::endl;
			return 0;
		}

		std::atomic<uint64_t> totalOrigBytes{0};
		std::atomic<uint64_t> totalGddsBytes{0};
		std::atomic<int> successCount{0};
		std::atomic<int> skippedCount{0};
		std::atomic<size_t> nextIndex{0};
		std::mutex printMutex;

		auto worker = [&]() {
			ComPtr<IDStorageCompressionCodec> codec;
			HRESULT hr = DStorageCreateCompressionCodec(DSTORAGE_COMPRESSION_FORMAT_GDEFLATE, 0, IID_PPV_ARGS(&codec));
			if (FAILED(hr) || !codec) {
				std::lock_guard<std::mutex> lock(printMutex);
				std::wcerr << L"[ERROR] Thread failed to initialize DirectStorage codec" << std::endl;
				return;
			}

			size_t total = ddsFiles.size();
			while (true) {
				size_t idx = nextIndex.fetch_add(1);
				if (idx >= total) break;

				const fs::path& ddsPath = ddsFiles[idx];
				fs::path gddsPath = ddsPath;
				gddsPath.replace_extension(L".gdds");

				uint64_t origSize = 0, gddsSize = 0;
				std::wstring skipReason;
				bool ok = CompressDdsToGdds(codec.Get(), ddsPath, gddsPath, origSize, gddsSize, skipReason, mipMode);

				if (ok) {
					totalOrigBytes += origSize;
					totalGddsBytes += gddsSize;
					int sCount = ++successCount;

					if (!keepOriginals) {
						DeleteFileW(NormalizePath(ddsPath).c_str());
					}

					double ratio = 100.0 * (1.0 - (static_cast<double>(gddsSize) / static_cast<double>(origSize)));
					{
						std::lock_guard<std::mutex> lock(printMutex);
						std::wcout << L"[" << (idx + 1) << L"/" << total << L"] [OK] "
							<< ddsPath.filename().wstring() << L" ("
							<< (origSize / 1024) << L" KB -> " << (gddsSize / 1024) << L" KB, -"
							<< std::fixed << std::setprecision(1) << ratio << L"%)" << std::endl;
					}
				} else {
					skippedCount++;
					{
						std::lock_guard<std::mutex> lock(printMutex);
						std::wcout << L"[" << (idx + 1) << L"/" << total << L"] [SKIP] "
							<< ddsPath.filename().wstring() << L": " << skipReason << std::endl;
					}
				}
			}
		};

		std::vector<std::thread> workers;
		workers.reserve(threadCount);
		for (unsigned int t = 0; t < threadCount; ++t) {
			workers.emplace_back(worker);
		}
		for (auto& w : workers) {
			if (w.joinable()) w.join();
		}

		if (updateIni) {
			std::wcout << L"\nReconciling .ini references with local files..." << std::endl;
			std::vector<fs::path> iniFiles;
			if (recursive) {
				FindFilesRecursiveWin32(targetDir.wstring(), L".ini", iniFiles);
			} else {
				std::wstring searchPath = NormalizePath(targetDir) + L"\\*";
				WIN32_FIND_DATAW fd;
				HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);
				if (hFind != INVALID_HANDLE_VALUE) {
					do {
						if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
							std::wstring fname = fd.cFileName;
							size_t dot = fname.rfind(L'.');
							if (dot != std::wstring::npos) {
								std::wstring ext = fname.substr(dot);
								for (auto& c : ext) c = towlower(c);
								if (ext == L".ini") iniFiles.push_back(targetDir / fname);
							}
						}
					} while (FindNextFileW(hFind, &fd));
					FindClose(hFind);
				}
			}

			int inisUpdated = 0;
			for (const auto& iniPath : iniFiles) {
				if (ReconcileIniReferences(iniPath)) inisUpdated++;
			}
			std::wcout << L"[INI] Reconciled " << inisUpdated << L" configuration files." << std::endl;
		}

		std::wcout << L"\n=== Final Summary ===" << std::endl;
		std::wcout << L"Converted: " << successCount.load() << L" | Preserved: " << skippedCount.load() << std::endl;
		uint64_t orig = totalOrigBytes.load();
		uint64_t gdds = totalGddsBytes.load();
		if (orig > 0) {
			double totalSaved = 100.0 * (1.0 - (static_cast<double>(gdds) / static_cast<double>(orig)));
			std::wcout << L"Original texture data: " << (orig / (1024 * 1024)) << L" MB (" 
				<< std::fixed << std::setprecision(2) << (orig / (1024.0 * 1024.0 * 1024.0)) << L" GB)" << std::endl;
			std::wcout << L"GDDS texture data:     " << (gdds / (1024 * 1024)) << L" MB (" 
				<< std::fixed << std::setprecision(2) << (gdds / (1024.0 * 1024.0 * 1024.0)) << L" GB)" << std::endl;
			std::wcout << L"Total storage freed:   " << ((orig - gdds) / (1024 * 1024)) << L" MB (" 
				<< std::fixed << std::setprecision(2) << ((orig - gdds) / (1024.0 * 1024.0 * 1024.0)) << L" GB, -" 
				<< std::fixed << std::setprecision(1) << totalSaved << L"%)" << std::endl;
		}
		return 0;
	}

	// -----------------------------------------------------------------------
	// Single File Mode (Auto-Toggle or Explicit)
	// -----------------------------------------------------------------------
	ComPtr<IDStorageCompressionCodec> codec;
	HRESULT hr = DStorageCreateCompressionCodec(DSTORAGE_COMPRESSION_FORMAT_GDEFLATE, 0, IID_PPV_ARGS(&codec));
	if (FAILED(hr) || !codec) {
		std::wcerr << L"[FATAL] Failed to initialize DirectStorage GDeflate codec" << std::endl;
		return 1;
	}

	fs::path inPath = targetPath;
	std::wstring inExt = inPath.extension().wstring();
	for (auto& c : inExt) c = towlower(c);

	bool decompressFile = explicitDecompress || (inExt == L".gdds");
	fs::path outPath = !explicitOutPath.empty() ? explicitOutPath : inPath;

	if (explicitOutPath.empty()) {
		if (decompressFile) outPath.replace_extension(L".dds");
		else outPath.replace_extension(L".gdds");
	}

	if (decompressFile) {
		std::wstring skipReason;
		if (DecompressGddsToDds(codec.Get(), inPath, outPath, skipReason)) {
			if (!keepOriginals && inPath != outPath) {
				DeleteFileW(NormalizePath(inPath).c_str());
			}
			std::wcout << L"[SUCCESS] Decompressed: " << inPath.filename().wstring() << L" -> " << outPath.filename().wstring() << std::endl;
			return 0;
		} else {
			std::wcerr << L"[FAILED] " << inPath.filename().wstring() << L": " << skipReason << std::endl;
			return 1;
		}
	} else {
		uint64_t origSize = 0, gddsSize = 0;
		std::wstring skipReason;
		if (CompressDdsToGdds(codec.Get(), inPath, outPath, origSize, gddsSize, skipReason, mipMode)) {
			if (!keepOriginals && inPath != outPath) {
				DeleteFileW(NormalizePath(inPath).c_str());
			}
			double ratio = 100.0 * (1.0 - (static_cast<double>(gddsSize) / static_cast<double>(origSize)));
			std::wcout << L"[SUCCESS] " << inPath.filename().wstring() << L" -> " << outPath.filename().wstring() << std::endl;
			std::wcout << L"Original size:   " << origSize << L" bytes (" << (origSize / 1024) << L" KB)" << std::endl;
			std::wcout << L"GDDS size:       " << gddsSize << L" bytes (" << (gddsSize / 1024) << L" KB)" << std::endl;
			std::wcout << L"Space reduction: " << std::fixed << std::setprecision(1) << ratio << L"%" << std::endl;
			return 0;
		} else {
			std::wcerr << L"[FAILED] " << inPath.filename().wstring() << L": " << skipReason << std::endl;
			return 1;
		}
	}
}
