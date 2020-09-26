FROM archlinux/base
WORKDIR /pshs/build
RUN pacman --noconfirm -Sy gcc meson pkgconf libevent qrencode miniupnpc
COPY . /pshs
RUN meson .. \
	-Dlibmagic=enabled \
	-Dqrencode=enabled \
	-Dssl=enabled \
	-Dupnp=enabled
RUN ninja install
CMD ["pshs"]
