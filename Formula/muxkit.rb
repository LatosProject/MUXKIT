class Muxkit < Formula
  desc "Lightweight terminal multiplexer written in C"
  homepage "https://github.com/LatosProject/muxkit"
  version "0.4.3"
  license "MIT"

  on_macos do
    if Hardware::CPU.arm?
      url "https://github.com/LatosProject/muxkit/releases/download/v#{version}/muxkit-v#{version}-macos-arm64.tar.gz"
      # sha256 ""
    else
      url "https://github.com/LatosProject/muxkit/releases/download/v#{version}/muxkit-v#{version}-macos-amd64.tar.gz"
      # sha256 ""
    end
  end

  on_linux do
    if Hardware::CPU.arm?
      url "https://github.com/LatosProject/muxkit/releases/download/v#{version}/muxkit-v#{version}-linux-arm64.tar.gz"
      # sha256 ""
    else
      url "https://github.com/LatosProject/muxkit/releases/download/v#{version}/muxkit-v#{version}-linux-amd64.tar.gz"
      # sha256 ""
    end
  end

  def install
    bin.install "muxkit"
  end

  test do
    assert_match "muxkit v#{version}", shell_output("#{bin}/muxkit -h", 0)
  end
end
