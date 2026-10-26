% experiment with frequencies available_graphics_toolkits
clear all;

Fref = 25e6;
Fgoal = 194.31415926e6;

% given Fgoal, find many approximations from sweeping R and MOD
% sweep R from sweeping Fpfd over reasonable range

% find all the R values available
Fpfdmin = 100e3; % limited by loop bandwidth
Fpfdmax = 1e6;  % limited by device hardware
Rmin = ceil(Fref / Fpfdmax);
Rmax = floor(Fref / Fpfdmin);
Rmax = min(Rmax, 1024); % limited by divider ratio

N = 1;
for R = Rmin:Rmax % sweep Fpfd through operational range
  Fpfd = Fref / R;
  INT = floor(Fgoal / Fpfd);
  if INT > 2^16
    fprintf(1, "INT too large: %d\n", INT);
    break;
  endif
  Fint = INT * Fpfd;
  Ffrac = Fgoal - Fint; % 0 to Fpfd
  [FRAC, MOD] = rat_max_den(Ffrac / Fpfd, 4095, 1e-6);
  if MOD > 4095
    fprintf(1, "MOD too large: %d\n", MOD);
    break;
  endif

  Fout = Fpfd * (INT + FRAC/MOD);
  if abs(Fout - Fgoal) < 1e2; % only save close results
    aR(N) = R;
    aINT(N) = INT;
    aMOD(N) = MOD;
    aFRAC(N) = FRAC;
    aFout(N) = Fout;
    N = N + 1;
  endif
endfor

if N == 1
  fprintf(1, "no results\n");
  return;
end
[Ferr, Nmin] = min(abs(aFout-Fgoal));
Fpfd = Fref/aR(Nmin);

fprintf(1, "Ferr %f, N %d, R %d, Fpfd %f, MOD %d, INT %d, FRAC %d, Fout %f\n", ...
            Ferr, Nmin, aR(Nmin), Fpfd, aMOD(Nmin), aINT(Nmin), aFRAC(Nmin), aFout(Nmin));
figure(1);
clf; hold on; grid on;
plot(aFout-Fgoal, aFRAC, '.');
title("Frequency vs FRAC");

figure(2);
clf; hold on; grid on;
plot(aFout-Fgoal, aR, '.');
title("Frequency vs R");

figure(3);
clf; hold on; grid on;
plot(aFout-Fgoal, aMOD, '.');
title("Frequency vs MOD");

figure(4);
clf; hold on; grid on;
plot(aFout-Fgoal, '.');
title("Frequency vs N");

figure(5);
hist(aFout-Fgoal, 2000);
title("Histogram of Frequency error");




